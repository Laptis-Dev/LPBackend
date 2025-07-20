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

#pragma once

#include <lpbackend/extern.hpp>
#include <semver.hpp>
#include <string>

namespace lpbackend
{
LPBACKEND_EXTERN extern const std::string release_version;
LPBACKEND_EXTERN extern const std::string branch_name;
LPBACKEND_EXTERN extern const std::string commit_hash;
LPBACKEND_EXTERN extern const std::string full_version;
LPBACKEND_EXTERN extern const std::int32_t protocol_version;
LPBACKEND_EXTERN extern const std::string protocol_version_name;
inline const semver::version semantic_version{[] constexpr {
    semver::version temp{};
    semver::parse(release_version, temp);
    return temp;
}()};
} // namespace lpbackend
