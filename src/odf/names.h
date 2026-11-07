/******************************************************************************
 * Copyright 2026 Michael Coutlakis
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

// Private: style names in ODF files. The model names a style as it is shown
// ("Text body", "Heading 1"); an ODF file names it with the characters an XML
// name can't hold spelled _xx_ in hex ("Text_20_body"), and a few of
// LibreOffice's own differ ("Standard" for the default paragraph style).

#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

#include "docsmithcpp/style.h"

namespace docsmith::odf
{

/// The builtins whose ODF name isn't their name spelled as above: ODF name,
/// then the model's.
inline const std::array<std::pair<std::string, std::string>, 2> &renamed_builtins()
{
    static const std::array<std::pair<std::string, std::string>, 2> names{{
        {"Standard", builtin::para.str()},
        {"Internet_20_link", builtin::link.str()},
    }};
    return names;
}

/// A name with the _xx_ escapes decoded ("Text_20_body" is "Text body").
inline std::string decode_name(std::string_view odf)
{
    auto hex = [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; };
    std::string out;
    for(std::size_t i = 0; i != odf.size(); ++i)
    {
        if(odf[i] == '_' && i + 3 < odf.size() && hex(odf[i + 1]) && hex(odf[i + 2]) &&
            odf[i + 3] == '_')
        {
            out += static_cast<char>(std::stoi(std::string(odf.substr(i + 1, 2)), nullptr, 16));
            i += 3;
        }
        else
            out += odf[i];
    }
    return out;
}

/// The model's name for a style named odf in a file.
inline std::string model_name(std::string_view odf)
{
    for(const auto &[from, to] : renamed_builtins())
        if(odf == from)
            return std::string(to);
    return decode_name(odf);
}

/// The name a file gives the style the model calls name: an XML name, with
/// other characters spelled _xx_. A name that is one already is kept, so a
/// name written by an earlier version of this library ("Text_20_body") or
/// made by the writer ("P1") stays as it is.
inline std::string odf_name(std::string_view name)
{
    for(const auto &[from, to] : renamed_builtins())
        if(name == to)
            return std::string(from);
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for(std::size_t i = 0; i != name.size(); ++i)
    {
        const auto c = static_cast<unsigned char>(name[i]);
        const bool letter = std::isalpha(c) || c == '_' || c >= 0x80;
        const bool ok = letter || (i != 0 && (std::isdigit(c) || c == '-' || c == '.'));
        if(ok)
            out += static_cast<char>(c);
        else
        {
            out += '_';
            out += digits[c >> 4];
            out += digits[c & 0xf];
            out += '_';
        }
    }
    return out;
}

} // namespace docsmith::odf
