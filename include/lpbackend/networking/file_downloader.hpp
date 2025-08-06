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

#include <fstream>

#include <fmt/format.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/url.hpp>

#include <lpbackend/extern.hpp>
#include <lpbackend/log.hpp>

namespace lpbackend::networking
{
class LPBACKEND_EXTERN file_downloader
{
  public:
    using executor_type = boost::asio::strand<boost::asio::io_context::executor_type>;
    using stream_type = typename boost::beast::tcp_stream::rebind_executor<executor_type>::other;

  private:
    boost::beast::net::ssl::context ssl_context_;
    logger lg_{channel_logger("file_downloader")};

    template <typename Stream>
    inline boost::asio::awaitable<std::vector<char>, executor_type> handle_request(Stream &stream,
                                                                                   const boost::urls::url_view url,
                                                                                   const std::string_view user_agent,
                                                                                   const std::string_view accept_mime)
    {

        {
            boost::beast::http::request<boost::beast::http::string_body> request{boost::beast::http::verb::get,
                                                                                 url.path(), 11};
            request.set(boost::beast::http::field::host, url.host());
            request.set(boost::beast::http::field::user_agent, user_agent);
            request.set(boost::beast::http::field::accept, accept_mime);
            request.set(boost::beast::http::field::connection, "close");
            co_await boost::beast::http::async_write(stream, request);
        }

        {
            boost::beast::flat_buffer buffer{};
            boost::beast::http::response<boost::beast::http::dynamic_body> response{};
            co_await boost::beast::http::async_read(stream, buffer, response);

            boost::beast::error_code ec{};
            if constexpr (requires(Stream &stream) {
                              stream.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                          })
            {
                // SSL stream
                stream.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            }
            else if constexpr (requires(Stream &stream) {
                                   stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                               })
            {
                // TCP stream
                stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            }

            if (ec == boost::asio::ssl::error::stream_truncated || ec == boost::beast::errc::not_connected)
            {
                ec = {};
            }

            if (ec)
            {
                throw boost::beast::system_error{ec};
            }
            std::vector<char> result{};
            for (const auto buffer : buffers_range_ref(response.body().data()))
            {
                result.append_range(std::span{reinterpret_cast<char *>(buffer.data()),
                                              reinterpret_cast<char *>(buffer.data()) + buffer.size()});
            }
            co_return result;
        }
    }

  public:
    inline file_downloader() : ssl_context_{boost::beast::net::ssl::context::tlsv12_client}
    {
        ssl_context_.set_default_verify_paths();
        ssl_context_.set_verify_mode(boost::beast::net::ssl::verify_peer);
    }

    /**
     * @brief Download file from a URL
     * file/http/https schemes are supported
     *
     * @param url to download from
     * @throws std::runtime_error on file reading error
     * @throws boost::beast::system_error on networking error
     */
    inline boost::asio::awaitable<std::vector<char>, executor_type> start_download(const boost::urls::url_view url,
                                                                                   const std::string_view user_agent,
                                                                                   const std::string_view accept_mime)
    {
        auto state{co_await boost::asio::this_coro::cancellation_state};
        auto executor{co_await boost::asio::this_coro::executor};
        if (static_cast<bool>(state.cancelled()))
        {
            co_return std::vector<char>{};
        }

        LPBACKEND_LOG(lg_, info) << "Downloading from " << url.data();

        if (url.scheme() == "http" || url.scheme() == "https")
        {
            boost::asio::ip::tcp::resolver resolver{executor};
            const auto resolution{co_await resolver.async_resolve(url.host(), url.port())};
            if (url.scheme() == "https")
            {
                boost::beast::ssl_stream<boost::beast::tcp_stream> stream{executor, ssl_context_};

                if (!SSL_set_tlsext_host_name(stream.native_handle(), url.host().c_str()))
                {
                    const boost::beast::error_code ec{static_cast<int>(ERR_get_error()),
                                                      boost::asio::error::get_ssl_category()};
                    throw boost::beast::system_error{ec};
                }

                co_await boost::beast::get_lowest_layer(stream).async_connect(resolution);
                co_await stream.async_handshake(boost::beast::net::ssl::stream_base::client);

                co_return co_await handle_request(stream, url, user_agent, accept_mime);
            }
            boost::beast::tcp_stream stream{executor};
            co_await stream.async_connect(resolution);
            co_return co_await handle_request(stream, url, user_agent, accept_mime);
        }

        else if (url.scheme() == "file")
        {
            std::ifstream file{url.path()};
            if (!file)
            {
                throw std::runtime_error{fmt::format("failed to download from {}", url.data())};
            }

            file.seekg(0, std::ios::end);
            const auto size{static_cast<size_t>(file.tellg())};
            file.seekg(0, std::ios::beg);

            std::vector<char> buffer{};
            buffer.resize(size);
            file.read(buffer.data(), size);

            if (!file.good())
            {
                throw std::runtime_error{fmt::format("failed to download from {}", url.data())};
            }

            co_return buffer;
        }

        else
        {
            throw std::runtime_error{fmt::format("unsupported URL scheme {}", url.scheme())};
        }
    }
};
} // namespace lpbackend::networking
