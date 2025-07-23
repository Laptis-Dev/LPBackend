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

#include <mutex>
#include <string>
#include <unordered_map>

#include <lpbackend/extern.hpp>
#include <lpbackend/log.hpp>
#include <lpbackend/networking/file_downloader.hpp>
#include <lpbackend/version.hpp>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>

namespace lpbackend::networking
{
class LPBACKEND_EXTERN mime_database
{
  public:
    using executor_type = boost::asio::strand<boost::asio::io_context::executor_type>;
    using stream_type = typename boost::beast::tcp_stream::rebind_executor<executor_type>::other;

  private:
    std::unordered_map<std::string, std::string> db_{
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"txt", "text/plain"},
        {"csv", "text/csv"},
        {"xml", "application/xml"},
        {"json", "application/json"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"gif", "image/gif"},
        {"webp", "image/webp"},
        {"ico", "image/x-icon"},
        {"svg", "image/svg+xml"},
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf", "font/ttf"},
        {"otf", "font/otf"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"tar", "application/x-tar"},
        {"gz", "application/gzip"},
        {"bz2", "application/x-bzip2"},
        {"7z", "application/x-7z-compressed"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"ogg", "audio/ogg"},
        {"mp4", "video/mp4"},
        {"webm", "video/webm"},
        {"doc", "application/msword"},
        {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {"wasm", "application/wasm"}};
    std::mutex db_mutex_;
    logger lg_{channel_logger("mime_database")};
    file_downloader downloader_;

    static inline const std::string default_mime{"application/octet-stream"};

  public:
    boost::asio::awaitable<void, executor_type> start_update(const boost::urls::url_view url)
    {
        auto state{co_await boost::asio::this_coro::cancellation_state};
        co_await boost::asio::this_coro::reset_cancellation_state(boost::asio::enable_total_cancellation());
        if (static_cast<bool>(state.cancelled()))
        {
            co_return;
        }

        LPBACKEND_LOG(lg_, info) << "Updating MIME database";

        auto file{co_await downloader_.start_download(url, user_agent, "application/json")};
        auto root{boost::json::parse(std::string_view{file.begin(), file.end()}).as_object()};

        // parsing database
        std::lock_guard<std::mutex> lock{db_mutex_};
        for (const auto &mime_pair : root)
        {
            const auto mime_type{mime_pair.key()};
            const auto mime_obj{mime_pair.value().as_object()};

            auto extentions_it{mime_obj.find("extensions")};
            if (extentions_it != mime_obj.end())
            {
                const auto &extensions{extentions_it->value().as_array()};
                for (const auto &ext : extensions)
                {
                    db_[ext.as_string().c_str()] = mime_type;
                }
            }
        }

        LPBACKEND_LOG(lg_, info) << fmt::format("Finished MIME database update ({} entries)", db_.size());
    }

    const std::string &get_mime_type(const std::string &ext)
    {
        std::lock_guard<std::mutex> lock{db_mutex_};
        if (!db_.contains(ext))
        {
            return default_mime;
        }
        return db_[ext];
    }
};
} // namespace lpbackend::networking
