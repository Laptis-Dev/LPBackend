/*
 * Copyright (c) 2025 Laptis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cxx_detect.h>

#include <fmt/format.h>

#include <boost/asio.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <lpbackend/config/lpbackend_config.hpp>
#include <lpbackend/lpbackend_server.hpp>
#include <lpbackend/plugin/plugin.hpp>

#if CXX_OS_WINDOWS
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
std::string read_password() noexcept
{
#if CXX_OS_WINDOWS
    HANDLE handle_stdin{GetStdHandle(STD_INPUT_HANDLE)};
    DWORD mode;
    GetConsoleMode(handle_stdin, &mode);
    SetConsoleMode(handle_stdin, mode & ~ENABLE_ECHO_INPUT);
#else
    termios oldt{};
    tcgetattr(STDIN_FILENO, &oldt);
    termios newt{oldt};
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
#endif
    std::string password{};
    std::getline(boost::nowide::cin, password);
#if CXX_OS_WINDOWS
    GetConsoleMode(handle_stdin, &mode);
#else
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
    boost::nowide::cout << "\n";
    return password;
}

void checked_context_run(boost::asio::io_context &context, logger &lg)
{
    try
    {
        context.run();
    }
    catch (const std::exception &e)
    {
        LPBACKEND_LOG(lg, fatal) << "Exception occurred on running I/O context" << e.what();
        throw;
    }
}
} // namespace

namespace lpbackend
{
const plugin::plugin_descriptor lpbackend_server::descriptor{.name = lpbackend_server::name,
                                                             .version = semantic_version,
                                                             .description = "Laptis Dev Forum Backend Core",
                                                             .authors = {"Laptis"},
                                                             .website = "https://github.com/Laptis-Dev/LPBackend",
                                                             .spdx_license = "MIT"};

const lpbackend::plugin::plugin_descriptor &lpbackend_server::get_descriptor() noexcept
{
    return descriptor;
}

lpbackend_server::lpbackend_server(const boost::program_options::variables_map &vm)
    : config_{}, vm_{vm}, ssl_context_{boost::asio::ssl::context::tlsv13}, task_group_{context_.get_executor()}
{
}

boost::asio::awaitable<void, lpbackend_server::executor_type> lpbackend_server::handle_signals()
{
    auto executor{co_await boost::asio::this_coro::executor};
    auto signal_set{boost::asio::signal_set{executor, SIGINT, SIGTERM}};
    auto signal{co_await signal_set.async_wait()};
    if (signal == SIGINT)
    {
        co_await stop();
    }
    else
    {
        executor.get_inner_executor().context().stop();
    }
}

boost::asio::awaitable<void, lpbackend_server::executor_type> lpbackend_server::start_accept()
{
    auto state{co_await boost::asio::this_coro::cancellation_state};
    auto executor{co_await boost::asio::this_coro::executor};
    acceptor_type acceptor{
        executor, boost::asio::ip::tcp::endpoint{boost::asio::ip::make_address(config_.networking.listen_address),
                                                 config_.networking.listen_port}};

    // Allow total cancellation to propagate to async operations
    co_await boost::asio::this_coro::reset_cancellation_state(boost::asio::enable_total_cancellation());

    while (!state.cancelled())
    {
        auto socket_executor{make_strand(executor.get_inner_executor())};
        auto [ec, socket]{co_await acceptor.async_accept(socket_executor, boost::asio::as_tuple)};

        if (ec == boost::asio::error::operation_aborted)
        {
            co_return;
        }

        if (ec)
        {
            throw boost::system::system_error{ec};
        }

        co_spawn(std::move(socket_executor), detect_session(stream_type{std::move(socket)}),
                 task_group_.adapt([this](const std::exception_ptr eptr) {
                     if (!eptr)
                     {
                         return;
                     }
                     try
                     {
                         rethrow_exception(eptr);
                     }
                     catch (std::exception &e)
                     {
                         LPBACKEND_LOG(lg_, error) << "Exception occured in session: " << e.what();
                     }
                 }));
    }
}

boost::asio::awaitable<void, lpbackend_server::executor_type> lpbackend_server::detect_session(stream_type stream)
{
    boost::beast::flat_buffer buffer{};

    // Allow total cancellation to change the cancellation state of this
    // coroutine, but only allow terminal cancellation to propagate to async
    // operations. This setting will be inherited by all child coroutines.
    co_await boost::asio::this_coro::reset_cancellation_state(boost::asio::enable_total_cancellation(),
                                                              boost::asio::enable_terminal_cancellation());

    // We want to be able to continue performing new async operations, such as
    // cleanups, even after the coroutine is cancelled. This setting will be
    // inherited by all child coroutines.
    co_await boost::asio::this_coro::throw_if_cancelled(false);

    using namespace std::literals;
    stream.expires_after(30s);

    auto ssl_detected{co_await boost::beast::async_detect_ssl(stream, buffer)};

    if (ssl_detected)
    {
        boost::asio::ssl::stream<stream_type> ssl_stream{std::move(stream), ssl_context_};

        auto bytes_transferred{
            co_await ssl_stream.async_handshake(boost::asio::ssl::stream_base::server, buffer.data())};

        buffer.consume(bytes_transferred);

        LPBACKEND_LOG(lg_, info) << "Accepting incoming HTTPS connection";
        co_await run_session(ssl_stream, buffer);

        if (!ssl_stream.lowest_layer().is_open())
        {
            co_return;
        }

        auto [ec]{co_await ssl_stream.async_shutdown(boost::asio::as_tuple)};
        if (ec && ec != boost::asio::ssl::error::stream_truncated)
        {
            throw boost::system::system_error{ec};
        }
    }
    else if (!ssl_detected && !config_.ssl.force_ssl)
    {
        LPBACKEND_LOG(lg_, info) << "Accepting incoming HTTP connection";
        co_await run_session(stream, buffer);
    }
    else if (!ssl_detected && config_.ssl.force_ssl)
    {
        LPBACKEND_LOG(lg_, error) << "Rejecting incoming HTTP connection (forcing SSL)";
        stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
    }
}

void lpbackend_server::initialize(lpbackend::plugin::plugin_manager &)
{
    // Load configuration
    try
    {
        config_.load();
    }
    catch (const boost::property_tree::json_parser_error &e)
    {
        LPBACKEND_LOG(lg_, fatal) << "Failed to parse JSON config: " << e.what();
    }

    if (!vm_.count("color") && !config_.logging.color_logging)
    {
        lpbackend::log::color_enabled = false;
        LPBACKEND_LOG(lg_, info) << "Disabled colored logging";
    }

    // Setup SSL context
    try
    {
        LPBACKEND_LOG(lg_, info) << "Loading SSL certificates";
        ssl_context_.set_password_callback(
            [this](std::size_t max_size, boost::asio::ssl::context::password_purpose purpose) -> std::string {
                std::string password{};
                do
                {
                    LPBACKEND_LOG(lg_, info) << fmt::format(
                        "Password needed for {} SSL certificates: ({} chars max)",
                        purpose == boost::asio::ssl::context_base::for_reading ? "reading" : "writing", max_size);
                    password = read_password();
                } while (password.length() > max_size);
                return password;
            });
        ssl_context_.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                                 boost::asio::ssl::context::single_dh_use);
        ssl_context_.use_certificate_chain_file(config_.ssl.certificate);
        ssl_context_.use_private_key_file(config_.ssl.private_key, boost::asio::ssl::context::file_format::pem);
        ssl_context_.use_tmp_dh_file(config_.ssl.tmp_dh);
    }
    catch (const boost::system::system_error &e)
    {
        LPBACKEND_LOG(lg_, fatal) << "Failed to load SSL certificates";
        throw;
    }

    create_directories(config_.http.doc_root);
}

void lpbackend_server::start()
{
    LPBACKEND_LOG(lg_, info) << "Starting LPBackend server";

    // Create and launch a listening coroutine
    co_spawn(make_strand(context_), start_accept(), task_group_.adapt([this](const std::exception_ptr eptr) {
        if (!eptr)
        {
            return;
        }
        try
        {
            rethrow_exception(eptr);
        }
        catch (std::exception &e)
        {
            LPBACKEND_LOG(lg_, error) << "Exception occured on starting accept: " << e.what();
        }
    }));

    // Create and launch a signal handler coroutine
    co_spawn(make_strand(context_), handle_signals(), boost::asio::detached);

    // Run the I/O service on the requested number of threads
    pool_.reserve(config_.asio.worker_threads - 1);
    for (std::size_t i{}; i < config_.asio.worker_threads; i++)
    {
        pool_.emplace_back([this] { checked_context_run(context_, lg_); });
    }
    checked_context_run(context_, lg_);

    // Block until all the threads exit
    for (auto &thread : pool_)
    {
        thread.join();
    }
}

boost::asio::awaitable<void, lpbackend_server::executor_type> lpbackend_server::stop()
{
    using namespace std::literals;

    LPBACKEND_LOG(lg_, info) << "Stopping LPBackend server";
    task_group_.emit(boost::asio::cancellation_type::total);

    LPBACKEND_LOG(lg_, info) << "Waiting child tasks to terminate for 10s";
    const auto [ec]{co_await task_group_.async_wait(boost::asio::as_tuple(boost::asio::cancel_after(10s)))};

    if (ec == boost::asio::error::operation_aborted) // Timed out
    {
        LPBACKEND_LOG(lg_, error) << "Terminating child tasks...";
        task_group_.emit(boost::asio::cancellation_type::terminal);
        co_await task_group_.async_wait();
    }
}

void lpbackend_server::terminate()
{
    LPBACKEND_LOG(lg_, info) << "Terminating LPBackend server";
    context_.stop();
}

lpbackend_server::~lpbackend_server()
{
    LPBACKEND_LOG(lg_, info) << "Destructing LPBackend server";
    config_.save();
}
} // namespace lpbackend
