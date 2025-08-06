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

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <lpbackend/extern.hpp>
#include <lpbackend/log.hpp>
#include <lpbackend/plugin/plugin.hpp>

namespace lpbackend::plugin
{
class plugin;
class LPBACKEND_EXTERN plugin_manager
{
  private:
    logger lg_{channel_logger("plugin_manager")};
    std::unordered_map<std::string, std::shared_ptr<plugin>> plugins_;
    std::mutex plugins_mutex_;

  public:
    plugin_manager();
    plugin_manager(const plugin_manager &) = delete;
    void register_plugin(plugin *plugin);
    void initialize_plugins();
    std::size_t unload_plugin(const std::string &name);
    std::shared_ptr<plugin> get_plugin(const std::string &name) const;
};
} // namespace lpbackend::plugin
