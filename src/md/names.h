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

// Private: the style names Markdown reads into and writes from. They are
// LibreOffice's own, so a parsed document looks right in an .odt too.

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include "docsmithcpp/style.h"

namespace docsmith::md::names
{

inline const style_name emphasis{"Emphasis"};
inline const style_name strong{"Strong_20_Emphasis"};
/// A paragraph in a block quote.
inline const style_name quotations{"Quotations"};
inline const std::string numbered_list{"Numbering_20_123"};
inline const std::string bullet_list{"List_20_1"};

/// A bookmark name as an HTML id (an anchor's id and a link's #fragment); ids
/// can't contain white space. HTML output does the same.
inline std::string anchor_id(std::string_view name)
{
    std::string out(name);
    std::replace_if(out.begin(), out.end(), [](unsigned char c) { return std::isspace(c); }, '_');
    return out;
}

} // namespace docsmith::md::names
