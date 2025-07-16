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

#include <filesystem>
#include <fmt/format.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <lpbackend/config/lpbackend_config.hpp>

namespace lpbackend::config
{
lpbackend_config::lpbackend_config() noexcept
    : logging{.color_logging = true}, networking{.listen_address = "0.0.0.0",
                                                 .http_enable = true,
                                                 .https_enable = false,
                                                 .listen_port_http = 80,
                                                 .listen_port_https = 443,
                                                 .timeout_milliseconds = 60000},
      ssl{.certificate = "./ssl/cert.pem", .private_key = "./ssl/key.pem", .tmp_dh = "./ssl/dh.pem"}
{
}

void lpbackend_config::load()
{
    LPBACKEND_LOG(lg_, info) << "Loading LPBackend configuration";
    if (!std::filesystem::exists(file_path))
    {
        LPBACKEND_LOG(lg_, warning) << fmt::format("Failed to find {}, initializing a new one", file_path);
        save();
        return;
    }
    boost::property_tree::ptree tree{};
    boost::property_tree::read_json(file_path, tree);

    logging.color_logging = tree.get("logging.color_logging", logging.color_logging);

    networking.listen_address = tree.get("networking.listen_address", networking.listen_address);
    networking.http_enable = tree.get("networking.http_enable", networking.http_enable);
    networking.https_enable = tree.get("networking.https_enable", networking.https_enable);
    networking.listen_port_http = tree.get("networking.listen_port_http", networking.listen_port_http);
    networking.listen_port_https = tree.get("networking.listen_port_https", networking.listen_port_https);
    networking.timeout_milliseconds = tree.get("networking.timeout_milliseconds", networking.timeout_milliseconds);

    ssl.certificate = tree.get("ssl.certificate", ssl.certificate);
    ssl.private_key = tree.get("ssl.private_key", ssl.private_key);
    ssl.tmp_dh = tree.get("ssl.tmp_dh", ssl.tmp_dh);
    ssl.tmp_dh = tree.get("ssl.tmp_dh", ssl.tmp_dh);
    save();
}

void lpbackend_config::save()
{
    LPBACKEND_LOG(lg_, info) << "Saving LPBackend configuration";
    boost::property_tree::ptree tree{};

    tree.put("logging.color_logging", logging.color_logging);

    tree.put("networking.listen_address", networking.listen_address);
    tree.put("networking.http_enable", networking.http_enable);
    tree.put("networking.https_enable", networking.https_enable);
    tree.put("networking.listen_port_http", networking.listen_port_http);
    tree.put("networking.listen_port_https", networking.listen_port_https);
    tree.put("networking.timeout_milliseconds", networking.timeout_milliseconds);

    tree.put("ssl.certificate", ssl.certificate);
    tree.put("ssl.private_key", ssl.private_key);
    tree.put("ssl.tmp_dh", ssl.tmp_dh);

    create_directories(std::filesystem::path{file_path}.parent_path());
    boost::property_tree::write_json(file_path, tree);
}
} // namespace lpbackend::config
