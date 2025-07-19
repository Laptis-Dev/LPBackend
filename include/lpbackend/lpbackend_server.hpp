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

#pragma once

#include <thread>

#include <fmt/format.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/program_options.hpp>

#include <lpbackend/asio/task_group.hpp>
#include <lpbackend/config/lpbackend_config.hpp>
#include <lpbackend/extern.hpp>
#include <lpbackend/log.hpp>
#include <lpbackend/plugin/plugin.hpp>
#include <lpbackend/plugin/plugin_descriptor.hpp>
#include <lpbackend/version.hpp>

namespace lpbackend
{
class LPBACKEND_EXTERN lpbackend_server : public lpbackend::plugin::plugin
{
  public:
    using executor_type = boost::asio::strand<boost::asio::io_context::executor_type>;
    using acceptor_type = typename boost::asio::ip::tcp::acceptor::rebind_executor<executor_type>::other;
    using stream_type = typename boost::beast::tcp_stream::rebind_executor<executor_type>::other;

  private:
    logger lg_;
    config::lpbackend_config config_;
    boost::program_options::variables_map vm_;
    boost::asio::io_context context_;
    boost::asio::ssl::context ssl_context_;
    std::vector<std::thread> pool_;
    asio::task_group task_group_;

    boost::asio::awaitable<void, executor_type> handle_signals();
    boost::asio::awaitable<void, executor_type> start_accept();
    boost::asio::awaitable<void, executor_type> detect_session(stream_type stream);

    // TODO: MIME type query
    boost::beast::string_view mime_type(boost::beast::string_view path)
    {
        using boost::beast::iequals;
        auto const ext{[&path] {
            auto const pos = path.rfind(".");
            if (pos == boost::beast::string_view::npos)
            {
                return boost::beast::string_view{};
            }
            return path.substr(pos);
        }()};
        if (iequals(ext, ".htm"))
            return "text/html";
        if (iequals(ext, ".html"))
            return "text/html";
        if (iequals(ext, ".php"))
            return "text/html";
        if (iequals(ext, ".css"))
            return "text/css";
        if (iequals(ext, ".txt"))
            return "text/plain";
        if (iequals(ext, ".js"))
            return "application/javascript";
        if (iequals(ext, ".json"))
            return "application/json";
        if (iequals(ext, ".xml"))
            return "application/xml";
        if (iequals(ext, ".swf"))
            return "application/x-shockwave-flash";
        if (iequals(ext, ".flv"))
            return "video/x-flv";
        if (iequals(ext, ".png"))
            return "image/png";
        if (iequals(ext, ".jpe"))
            return "image/jpeg";
        if (iequals(ext, ".jpeg"))
            return "image/jpeg";
        if (iequals(ext, ".jpg"))
            return "image/jpeg";
        if (iequals(ext, ".gif"))
            return "image/gif";
        if (iequals(ext, ".bmp"))
            return "image/bmp";
        if (iequals(ext, ".ico"))
            return "image/vnd.microsoft.icon";
        if (iequals(ext, ".tiff"))
            return "image/tiff";
        if (iequals(ext, ".tif"))
            return "image/tiff";
        if (iequals(ext, ".svg"))
            return "image/svg+xml";
        if (iequals(ext, ".svgz"))
            return "image/svg+xml";
        return "application/text";
    }

    // Append an HTTP rel-path to a local filesystem path.
    // The returned path is normalized for the platform.
    std::string path_cat(boost::beast::string_view base, boost::beast::string_view path)
    {
        if (base.empty())
        {
            return std::string{path};
        }
        std::string result{base};
#ifdef BOOST_MSVC
        char constexpr path_separator = '\\';
        if (result.back() == path_separator)
            result.resize(result.size() - 1);
        result.append(path.data(), path.size());
        for (auto &c : result)
            if (c == '/')
                c = path_separator;
#else
        constexpr char path_separator{'/'};
        if (result.back() == path_separator)
        {
            result.resize(result.size() - 1);
        }
        result.append(path.data(), path.size());
#endif
        return result;
    }

    template <typename Stream>
    boost::asio::awaitable<void, executor_type> run_session(Stream &stream, boost::beast::flat_buffer &buffer)
    {
        auto state{co_await boost::asio::this_coro::cancellation_state};

        while (!state.cancelled())
        {
            boost::beast::http::request_parser<boost::beast::http::string_body> parser{};

            auto [ec, _]{co_await boost::beast::http::async_read(stream, buffer, parser, boost::asio::as_tuple)};
            if (ec == boost::beast::http::error::end_of_stream)
            {
                co_return;
            }

            if (boost::beast::websocket::is_upgrade(parser.get()))
            {
                boost::beast::get_lowest_layer(stream).expires_never();
                // co_await run_websocket_session(stream, buffer, parser.release());
                co_return;
            }

            auto res{handle_request(parser.release())};
            if (!res.keep_alive())
            {
                co_await boost::beast::async_write(stream, std::move(res));
                co_return;
            }

            co_await boost::beast::async_write(stream, std::move(res));
        }
    }

    template <typename Body, typename Allocator>
    boost::beast::http::message_generator handle_request(
        boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>> &&req)
    {
        // TODO: Flexible request handling
        // Returns a bad request response
        auto const bad_request{[&req](const boost::beast::string_view why) {
            boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::bad_request,
                                                                              req.version()};
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = std::string{why};
            res.prepare_payload();
            return res;
        }};

        // Returns a not found response
        auto const not_found{[&req](const boost::beast::string_view target) {
            boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::not_found,
                                                                              req.version()};
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = fmt::format("The resource \"{}\" was not found.", target);
            res.prepare_payload();
            return res;
        }};

        // Returns a server error response
        auto const server_error{[&req](const boost::beast::string_view what) {
            boost::beast::http::response<boost::beast::http::string_body> res{
                boost::beast::http::status::internal_server_error, req.version()};
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = fmt::format("An error occurred: \"{}\"", what);
            res.prepare_payload();
            return res;
        }};

        // Make sure we can handle the method
        if (req.method() != boost::beast::http::verb::get && req.method() != boost::beast::http::verb::head)
        {
            return bad_request("Unknown HTTP-method");
        }

        // Request path must be absolute and not contain "..".
        if (req.target().empty() || req.target()[0] != '/' ||
            req.target().find("..") != boost::beast::string_view::npos)
        {
            return bad_request("Illegal request-target");
        }

        // Build the path to the requested file
        auto path{path_cat(config_.fields.http.doc_root.string(), req.target())};
        if (req.target().back() == '/')
            path.append("index.html");

        // Attempt to open the file
        boost::beast::error_code ec{};
        boost::beast::http::file_body::value_type body{};
        body.open(path.c_str(), boost::beast::file_mode::scan, ec);

        // Handle the case where the file doesn't exist
        if (ec == boost::beast::errc::no_such_file_or_directory)
            return not_found(req.target());

        // Handle an unknown error
        if (ec)
            return server_error(ec.message());

        // Cache the size since we need it after the move
        const auto size{body.size()};

        // Respond to HEAD request
        if (req.method() == boost::beast::http::verb::head)
        {
            boost::beast::http::response<boost::beast::http::empty_body> res{boost::beast::http::status::ok,
                                                                             req.version()};
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, mime_type(path));
            res.content_length(size);
            res.keep_alive(req.keep_alive());
            return res;
        }

        // Respond to GET request
        boost::beast::http::response<boost::beast::http::file_body> res{
            std::piecewise_construct, std::make_tuple(std::move(body)),
            std::make_tuple(boost::beast::http::status::ok, req.version())};
        res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return res;
    }

  public:
    static constexpr auto name{"lpbackend::server"};
    static const lpbackend::plugin::plugin_descriptor descriptor;
    explicit lpbackend_server(const boost::program_options::variables_map &vm);
    const lpbackend::plugin::plugin_descriptor &get_descriptor() noexcept override;
    void initialize(lpbackend::plugin::plugin_manager &manager) override;
    void start();

    /**
     * @brief Emit cancellations to stop the server
     */
    boost::asio::awaitable<void, executor_type> stop();

    /**
     * @brief Terminate the I/O service
     */
    void terminate();

    ~lpbackend_server();
};
} // namespace lpbackend
