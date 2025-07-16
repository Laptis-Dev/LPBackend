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

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/program_options.hpp>

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
  private:
    logger lg_;
    config::lpbackend_config config_;
    boost::program_options::variables_map vm_;
    boost::asio::io_context context_;
    boost::asio::ssl::context ssl_context_;

  public:
    static constexpr auto name{"lpbackend::server"};
    static const lpbackend::plugin::plugin_descriptor descriptor;
    explicit lpbackend_server(const boost::program_options::variables_map &vm);
    const lpbackend::plugin::plugin_descriptor &get_descriptor() noexcept override;
    void initialize(lpbackend::plugin::plugin_manager &manager) override;
    void start();
    void stop();
    ~lpbackend_server();
};
} // namespace lpbackend
