/*
 * Copyright (c) 2025 Laptis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
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

#include <exception>
#include <iostream>
#include <memory>

#include <boost/nowide/args.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/program_options.hpp>
#include <cxx_detect.h>
#include <fmt/format.h>

#include <lpbackend/log.hpp>
#include <lpbackend/lpbackend_server.hpp>
#include <lpbackend/plugin/plugin_manager.hpp>
#include <lpbackend/version.hpp>

#if CXX_OS_WINDOWS
#include <windows.h>
#endif

namespace
{
auto lpbackend_logo{R"( __    ____  ____   ___    ___ __ __  ____ __  __ ____)"
                    "\n"
                    R"(||    || \\ || )) // \\  //   || // ||    ||\ || || \\)"
                    "\n"
                    R"(||    ||_// ||=)  ||=|| ((    ||<<  ||==  ||\\|| ||  )))"
                    "\n"
                    R"(||__| ||    ||_)) || ||  \\__ || \\ ||___ || \|| ||_//)"
                    "\n"};

#if CXX_OS_WINDOWS
bool enable_ansi_support() noexcept
{
    const auto out{GetStdHandle(STD_OUTPUT_HANDLE)};
    if (out == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    DWORD mode{};
    if (!GetConsoleMode(out, &mode))
    {
        return false;
    }
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(out, mode))
    {
        return false;
    }
    return true;
}
#endif

std::unique_ptr<lpbackend::plugin::plugin_manager> manager;
} // namespace

int main(int argc, char *argv[])
{
    using namespace std::literals;
#if CXX_OS_WINDOWS
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    setlocale(LC_ALL, ".utf-8");
#if !defined(LPBACKEND_NOLOGO)
    boost::nowide::cout << lpbackend_logo << std::flush;
#endif
    lpbackend::log::initialize_logging_system();
    logger lg{};
#if CXX_OS_WINDOWS
    if (!enable_ansi_support())
    {
        lpbackend::log::color_enabled = false;
        LPBACKEND_LOG(lg, warning) << "Failed to enable ANSI escape sequence support, colored logging is disabled";
    }
    else
    {
        LPBACKEND_LOG(lg, trace) << "Enabled ANSI escape sequence support";
    }
#endif
    LPBACKEND_LOG(lg, trace) << "Logging system initialized";

    LPBACKEND_LOG(lg, info) << lpbackend::full_version;

#if !defined(NDEBUG) || defined(_DEBUG)
    {
        std::stringstream ss{};
        for (int i{}; i < argc; i++)
        {
            ss << fmt::format("[{}]:\"{}\" ", i, argv[i]);
        }
        LPBACKEND_LOG(lg, debug) << "Console argument: " << ss.str();
    }
#endif

    boost::program_options::options_description desc{"Plasma Usage"};
    desc.add_options()("help", "Show the help")("version", "Show the version only")(
        "init", "Initialize configurations only")("color", "Enable colored logging");
    boost::program_options::variables_map vm{};
    try
    {
        boost::nowide::args _{argc, argv};
        store(boost::program_options::parse_command_line(argc, argv, desc), vm);
        notify(vm);
    }
    catch (const std::exception &e)
    {
        LPBACKEND_LOG(lg, fatal) << "Failed to parse command line: " << e.what();
        return 1;
    }

    if (vm.count("version"))
    {
        return 0;
    }

    if (vm.count("help"))
    {
        std::cerr << desc << std::endl;
        return 1;
    }

    manager = std::make_unique<lpbackend::plugin::plugin_manager>();
    manager->register_plugin(new lpbackend::lpbackend_server{vm});
    manager->initialize_plugins();
    if (vm.count("init"))
    {
        LPBACKEND_LOG(lg, info) << "Initialized configurations";
        return 0;
    }
    dynamic_cast<lpbackend::lpbackend_server *>(manager->get_plugin(lpbackend::lpbackend_server::name).get())->start();
    return 0;
}
