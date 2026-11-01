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
#include <chrono>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

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

/// v to at most three decimals, e.g. "0.5" for a length in points.
inline std::string to_string(double v)
{
    const double rounded =
        static_cast<double>(static_cast<long long>(v * 1000 + (v < 0 ? -0.5 : 0.5))) / 1000;
    char buf[32];
    const auto [end, ec] = std::to_chars(buf, buf + sizeof buf, rounded);
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

/// Today's date in UTC as YYYY-MM-DD.
inline std::string today()
{
    using namespace std::chrono;
    const year_month_day ymd{floor<days>(system_clock::now())};
    return zero_pad(static_cast<int>(ymd.year()), 4) + "-" +
        zero_pad(static_cast<unsigned>(ymd.month()), 2) + "-" +
        zero_pad(static_cast<unsigned>(ymd.day()), 2);
}

/// An xsd:dateTime ("2025-06-16T13:50:22.567", optionally ending in Z or
/// +hh:mm), to the second. Without a zone it is read as UTC.
inline std::optional<std::chrono::sys_seconds> parse_datetime(std::string_view s)
{
    using namespace std::chrono;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
    const std::string str(s);
    if(std::sscanf(str.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) < 3)
        return std::nullopt;
    const year_month_day ymd{year{y}, month(static_cast<unsigned>(mo)), day(static_cast<unsigned>(d))};
    if(!ymd.ok())
        return std::nullopt;
    sys_seconds t = sys_days{ymd} + hours{h} + minutes{mi} + seconds{sec};

    // The zone follows the time (and any fraction of a second).
    if(const auto tpos = s.find('T'); tpos != std::string_view::npos)
        if(const auto zpos = s.find_first_of("+-", tpos); zpos != std::string_view::npos)
        {
            int zh = 0, zm = 0;
            if(std::sscanf(str.c_str() + zpos + 1, "%d:%d", &zh, &zm) >= 1)
                t -= (s[zpos] == '-' ? -1 : 1) * (hours{zh} + minutes{zm});
        }
    return t;
}

/// t as an xsd:dateTime in UTC, e.g. "2025-06-16T13:50:22Z".
inline std::string format_datetime(std::chrono::sys_seconds t)
{
    using namespace std::chrono;
    const auto days = floor<std::chrono::days>(t);
    const year_month_day ymd{days};
    const hh_mm_ss hms{t - days};
    return zero_pad(static_cast<int>(ymd.year()), 4) + "-" +
        zero_pad(static_cast<unsigned>(ymd.month()), 2) + "-" +
        zero_pad(static_cast<unsigned>(ymd.day()), 2) + "T" + zero_pad(hms.hours().count(), 2) +
        ":" + zero_pad(hms.minutes().count(), 2) + ":" + zero_pad(hms.seconds().count(), 2) + "Z";
}

} // namespace docsmith::detail
