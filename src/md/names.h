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

// Private: names Markdown reads and writes. Its styles are builtins (see
// builtin::emphasis and the others).

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace docsmith::md::names
{

/// A bookmark name as an HTML id (an anchor's id and a link's #fragment); ids
/// can't contain white space. HTML output does the same.
inline std::string anchor_id(std::string_view name)
{
    std::string out(name);
    std::replace_if(out.begin(), out.end(), [](unsigned char c) { return std::isspace(c); }, '_');
    return out;
}

} // namespace docsmith::md::names
