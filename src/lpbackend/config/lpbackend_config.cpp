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
#include <fstream>

#include <fmt/format.h>

#include <boost/json.hpp>
#include <boost/pfr.hpp>

#include <lpbackend/config/lpbackend_config.hpp>
#include <lpbackend/config/pretty_print.hpp>

namespace lpbackend::config
{
void lpbackend_config::load()
{
    LPBACKEND_LOG(lg_, info) << "Loading LPBackend configuration";
    if (!std::filesystem::exists(file_path))
    {
        LPBACKEND_LOG(lg_, warning) << fmt::format("Failed to find {}, initializing a new one", file_path);
        save();
        return;
    }
    std::ifstream file_stream{file_path};
    auto root{boost::json::parse(file_stream)};
    file_stream.close();

    boost::pfr::for_each_field_with_name(fields, [this, &root](const std::string_view section_name, auto &section) {
        boost::pfr::for_each_field_with_name(
            section, [this, section_name, &root](const std::string_view field_name, auto &field) {
                auto pointer{fmt::format("/{}/{}", section_name, field_name)};
                if (!root.try_at_pointer(pointer))
                {
                    LPBACKEND_LOG(lg_, warning) << fmt::format("Failed to read {} from config", pointer);
                    return;
                }
                if constexpr (std::is_integral_v<std::decay_t<decltype(field)>>)
                {
                    field = value_to<std::decay_t<decltype(field)>>(root.at_pointer(pointer));
                }
                else if constexpr (std::is_floating_point_v<std::decay_t<decltype(field)>>)
                {
                    field = root.at_pointer(pointer).as_double();
                }
                else if constexpr (std::is_same_v<std::decay_t<decltype(field)>, bool>)
                {
                    field = root.at_pointer(pointer).as_bool();
                }
                else if constexpr (std::is_same_v<std::decay_t<decltype(field)>, std::string>)
                {
                    field = root.at_pointer(pointer).as_string();
                }
                else if constexpr (std::is_same_v<std::decay_t<decltype(field)>, std::filesystem::path>)
                {
                    field = std::filesystem::path{std::string{root.at_pointer(pointer).as_string()}};
                }
                else if constexpr (std::is_same_v<std::decay_t<decltype(field)>, boost::urls::url>)
                {
                    field = boost::urls::url{std::string{root.at_pointer(pointer).as_string()}};
                }
                else if constexpr (requires(std::decay_t<decltype(field)> t) {
                                       []<typename Tv>(std::vector<Tv>) {}(t);
                                   }) // is std::vector
                {
                    field = std::vector{root.at_pointer(pointer).as_array()};
                }
            });
    });

    save();
}

void lpbackend_config::save()
{
    LPBACKEND_LOG(lg_, info) << "Saving LPBackend configuration";

    boost::json::value root{};
    boost::pfr::for_each_field_with_name(fields, [&root](const std::string_view section_name, auto &section) {
        boost::pfr::for_each_field_with_name(
            section, [section_name, &root](const std::string_view field_name, auto &field) {
                auto pointer{fmt::format("/{}/{}", section_name, field_name)};
                if constexpr (std::is_same_v<std::decay_t<decltype(field)>, std::filesystem::path>)
                {
                    root.set_at_pointer(pointer, field.string());
                }
                else if constexpr (std::is_same_v<std::decay_t<decltype(field)>, boost::urls::url>)
                {
                    root.set_at_pointer(pointer, field.c_str());
                }
                else
                {
                    root.set_at_pointer(pointer, field);
                }
            });
    });

    create_directories(std::filesystem::path{file_path}.parent_path());
    std::ofstream file_stream{file_path};
    pretty_print(file_stream, root);
    file_stream.close();
}
} // namespace lpbackend::config
