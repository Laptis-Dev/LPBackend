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
#include <lpbackend/networking/mime_database.hpp>
#include <lpbackend/networking/request_handler.hpp>
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
    networking::request_handler request_handler_;
    networking::mime_database mime_database_;
    boost::program_options::variables_map vm_;
    boost::asio::io_context context_;
    boost::asio::ssl::context ssl_context_;
    std::vector<std::thread> pool_;
    asio::task_group task_group_;

    boost::asio::awaitable<void, executor_type> handle_signals();
    boost::asio::awaitable<void, executor_type> start_accept();
    boost::asio::awaitable<void, executor_type> detect_session(stream_type stream);

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
