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

// Private: the number each list item shows, for the writers that write the
// numbers themselves (HTML, Markdown).

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "docsmithcpp/list.h"

namespace docsmith::detail
{

/// The list style's numbering at a nesting level (1 at the top), or nullptr
/// for a bulleted (or unstyled) level.
inline const number_level *numbering(const list_style *ls, int level)
{
    if(!ls)
        return nullptr;
    for(const auto &s : ls->levels)
        if(std::visit([](const auto &x) { return x.level; }, s) == level)
            return std::get_if<number_level>(&s);
    return nullptr;
}

/// Numbers lists in document order, so that a list can continue the count of
/// the last one before it with the same list style at the same level.
class list_counter
{
public:
    /// The number each item of l shows, none for an unnumbered item. num is
    /// the numbering of ls at level (1 at the top).
    std::vector<std::optional<int>> numbers(
        const list &l, const list_style *ls, const number_level &num, int level)
    {
        int n = num.start_from;
        if(auto it = m_next.find({ls, level}); l.continue_numbering && it != m_next.end())
            n = it->second;
        std::vector<std::optional<int>> out;
        for(const auto &item : l.items())
        {
            if(!item.numbered)
            {
                out.emplace_back();
                continue;
            }
            if(item.start_value)
                n = *item.start_value;
            out.emplace_back(n++);
        }
        m_next[{ls, level}] = n;
        return out;
    }

private:
    std::map<std::pair<const list_style *, int>, int> m_next; //!< Next number by style, level
};

} // namespace docsmith::detail
