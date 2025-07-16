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
    : config_{}, vm_{vm}, ssl_context_{boost::asio::ssl::context::tlsv13}
{
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

    if (config_.networking.https_enable)
    {
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
            ssl_context_.set_options(boost::asio::ssl::context::default_workarounds |
                                     boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
            ssl_context_.use_certificate_chain_file(config_.ssl.certificate);
            ssl_context_.use_private_key_file(config_.ssl.private_key, boost::asio::ssl::context::file_format::pem);
            ssl_context_.use_tmp_dh_file(config_.ssl.tmp_dh);
        }
        catch (const boost::system::system_error &e)
        {
            LPBACKEND_LOG(lg_, error) << "Failed to load SSL certificates, HTTPS would be disabled";
            config_.networking.https_enable = false;
        }
    }
}

void lpbackend_server::start()
{
    LPBACKEND_LOG(lg_, info) << "Starting LPBackend server";
    context_.run();
}

void lpbackend_server::stop()
{
    LPBACKEND_LOG(lg_, info) << "Stopping LPBackend server";
    context_.stop();
}

lpbackend_server::~lpbackend_server()
{
    LPBACKEND_LOG(lg_, info) << "Destructing LPBackend server";
    config_.save();
}
} // namespace lpbackend
