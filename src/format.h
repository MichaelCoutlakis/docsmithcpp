/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#pragma once

// Private: the few number formats the writers need, without a formatting
// library.

#include <charconv>
#include <string>

#include "docsmithcpp/style.h"

namespace docsmith::detail
{

/// The shortest decimal form that reads back as v, e.g. "12" or "10.5".
inline std::string to_string(float v)
{
    char buf[32];
    const auto [end, ec] = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, end);
}

/// n in decimal, padded with leading zeros to at least width digits.
inline std::string zero_pad(long long n, std::size_t width)
{
    std::string s = std::to_string(n < 0 ? -n : n);
    if(s.size() < width)
        s.insert(0, width - s.size(), '0');
    return n < 0 ? "-" + s : s;
}

/// "#rrggbb", as both CSS and ODF write colours.
inline std::string to_hex(const colour &c)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string s = "#";
    for(const unsigned v : {c.m_r, c.m_g, c.m_b})
    {
        s += digits[v >> 4];
        s += digits[v & 0xf];
    }
    return s;
}

} // namespace docsmith::detail
