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

#include <map>
#include <memory>

#include <fmt/format.h>

#include <lpbackend/plugin/plugin.hpp>
#include <lpbackend/plugin/plugin_loading_exception.hpp>
#include <lpbackend/plugin/plugin_manager.hpp>

namespace lpbackend::plugin
{
plugin_manager::plugin_manager() = default;

void plugin_manager::register_plugin(plugin *plugin)
{
    std::lock_guard<std::mutex> lock{plugin_mutex_};
    if (plugin == nullptr)
    {
        throw plugin_loading_exception{"Trying to register a null plugin"};
    }
    LPBACKEND_LOG(lg_, info) << "Registering plugin " << plugin->get_descriptor().name << " "
                             << plugin->get_descriptor().version.to_string();
    const auto hash{std::hash<std::string>{}(plugin->get_descriptor().name)};
    LPBACKEND_LOG(lg_, trace) << "Plugin " << plugin->get_descriptor().name << " name hash \"" << std::hex << hash
                              << std::dec << "\"";
    if (plugins_.contains(hash))
    {
        throw plugin_loading_exception{"Trying to register a duplicated plugin"};
    }
    plugins_.insert(std::pair{hash, std::shared_ptr<class plugin>{plugin}});
}

void plugin_manager::initialize_plugins()
{
    std::lock_guard<std::mutex> lock{plugin_mutex_};
    for (auto &[_, plugin] : plugins_)
    {
        if (plugin->initialized_)
        {
            continue;
        }
        for (const auto &conflict : plugin->get_descriptor().conflicts)
        {
            if (plugins_.contains(std::hash<std::string>{}(conflict.name)))
            {
                LPBACKEND_LOG(lg_, error) << fmt::format("Detected conflict plugin {} while loading {}", conflict.name,
                                                         plugin->get_descriptor().name);
                throw plugin_loading_exception{"Plugin conflict detected"};
            }
        }
        auto initialize{[this](auto &&self, auto plugin) -> void {
            if (plugin->initialized_)
            {
                return;
            }
            plugin->initialized_ = true; // To prevent infinite recursion
            LPBACKEND_LOG(lg_, info) << "Initializing plugin " << plugin->get_descriptor().name << " "
                                     << plugin->get_descriptor().version.to_string();
            try
            {
                for (const auto &dependency : plugin->get_descriptor().dependencies)
                {
                    const auto hash{std::hash<std::string>{}(dependency.name)};
                    if (!plugins_.contains(hash))
                    {
                        LPBACKEND_LOG(lg_, error) << fmt::format("Dependency plugin {} not found while loading {}",
                                                                 dependency.name, plugin->get_descriptor().name);
                        throw plugin_loading_exception{"Plugin dependency not found"};
                    }
                    self(self, plugins_[hash]); // Throws std::out_of_range if dependency not found
                }
                for (const auto &optional_dependency : plugin->get_descriptor().optional_dependencies)
                {
                    if (const auto hash{std::hash<std::string>{}(optional_dependency.name)}; plugins_.contains(hash))
                    {
                        self(self, plugins_[hash]);
                    }
                    else
                    {
                        LPBACKEND_LOG(lg_, warning)
                            << "Optional dependency " << optional_dependency.name << " not found";
                    }
                }
                plugin->initialize(*this);
            }
            catch (...)
            {
                plugin->initialized_ = false;
                throw;
            }
        }};
        initialize(initialize, plugin);
    }
}

std::size_t plugin_manager::unload_plugin(const std::string &name)
{
    std::lock_guard<std::mutex> lock{plugin_mutex_};
    const auto hash{std::hash<std::string>{}(name)};
    if (plugins_.at(hash)) // throws std::out_of_range if not found
    {
        for (const auto &[_, plugin] : plugins_)
        {
            for (const auto &dependency : plugin->get_descriptor().dependencies)
            {
                if (dependency.name == name)
                {
                    throw plugin_loading_exception{"Failed to unload a depended plugin"};
                }
            }
        }
    }
    return plugins_.erase(hash);
}

std::shared_ptr<plugin> plugin_manager::get_plugin(const std::string &name) const
{
    return plugins_.at(std::hash<std::string>{}(name)); // Copies the shared_ptr
}
} // namespace lpbackend::plugin
