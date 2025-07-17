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
#include <thread>

#include <fmt/format.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <lpbackend/config/lpbackend_config.hpp>

namespace lpbackend::config
{
lpbackend_config::lpbackend_config() noexcept
    : logging{.color_logging = true},
      networking{.listen_address = "0.0.0.0", .listen_port = 443, .timeout_milliseconds = 60000},
      ssl{.certificate = "./ssl/cert.pem",
          .private_key = "./ssl/key.pem",
          .tmp_dh = "./ssl/dh.pem",
          .force_ssl = false},
      asio{.worker_threads = std::thread::hardware_concurrency()}
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
    read_json(file_path, tree);

    logging.color_logging = tree.get("logging.color_logging", logging.color_logging);

    networking.listen_address = tree.get("networking.listen_address", networking.listen_address);
    networking.listen_port = tree.get("networking.listen_port", networking.listen_port);
    networking.timeout_milliseconds = tree.get("networking.timeout_milliseconds", networking.timeout_milliseconds);

    ssl.certificate = tree.get("ssl.certificate", ssl.certificate);
    ssl.private_key = tree.get("ssl.private_key", ssl.private_key);
    ssl.tmp_dh = tree.get("ssl.tmp_dh", ssl.tmp_dh);
    ssl.force_ssl = tree.get("ssl.force_ssl", ssl.force_ssl);

    asio.worker_threads = tree.get("asio.worker_threads", asio.worker_threads);

    http.doc_root = tree.get("http.doc_root", http.doc_root);

    save();
}

void lpbackend_config::save()
{
    LPBACKEND_LOG(lg_, info) << "Saving LPBackend configuration";
    boost::property_tree::ptree tree{};

    tree.put("logging.color_logging", logging.color_logging);

    tree.put("networking.listen_address", networking.listen_address);
    tree.put("networking.listen_port", networking.listen_port);
    tree.put("networking.timeout_milliseconds", networking.timeout_milliseconds);

    tree.put("ssl.certificate", ssl.certificate);
    tree.put("ssl.private_key", ssl.private_key);
    tree.put("ssl.tmp_dh", ssl.tmp_dh);
    tree.put("ssl.force_ssl", ssl.force_ssl);

    tree.put("asio.worker_threads", asio.worker_threads);

    tree.put("http.doc_root", http.doc_root);

    create_directories(std::filesystem::path{file_path}.parent_path());
    write_json(file_path, tree);
}
} // namespace lpbackend::config
