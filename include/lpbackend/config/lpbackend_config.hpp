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

#include <filesystem>
#include <thread>

#include <lpbackend/config/config.hpp>
#include <lpbackend/log.hpp>

namespace lpbackend::config
{
class lpbackend_config : public config
{
  private:
    logger lg_;

  public:
    static constexpr auto file_path{"./config/lpbackend.json"};

    struct
    {
        struct
        {
            bool color_logging{true};
        } logging;

        struct
        {
            std::string listen_address{"0.0.0.0"};
            boost::asio::ip::port_type listen_port{443};
            std::uint64_t timeout_milliseconds{5000};
        } networking;

        struct
        {
            std::filesystem::path certificate{"./ssl/cert.pem"};
            std::filesystem::path private_key{"./ssl/key.pem"};
            std::filesystem::path tmp_dh{"./ssl/dh.pem"};
            bool force_ssl{false};
        } ssl;

        struct
        {
            std::uint64_t worker_threads{std::thread::hardware_concurrency()};
        } asio;

        struct
        {
            std::filesystem::path doc_root{"./docroot"};
            std::string fallback_file{"home.html"};
        } http;
    } fields;

    /**
     * @brief Loads the configuration
     *
     * @throws boost::system::system_error on JSON parsing failure
     */
    void load() override;

    /**
     * @brief Saves the configuration
     *
     * @throws boost::system::system_error on JSON parsing failure
     */
    void save() override;
};
} // namespace lpbackend::config
