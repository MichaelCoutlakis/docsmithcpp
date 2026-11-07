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
#include <optional>
#include <vector>

#include "docsmithcpp/nodes_fwd.h"
#include "docsmithcpp/style.h"

namespace docsmith
{

// ---------------------------------------------------------------------------
// List content nodes (block level)
// ---------------------------------------------------------------------------
class list_item : public has_children<block_node, list_item, true>
{
public:
    list_item() = default;
    template <typename... Args>
        requires content_args<list_item, block_node, Args...>
    list_item(Args &&...content)
    {
        (this->add(std::forward<Args>(content)), ...);
    }

    /// Whether the item shows its number or bullet. An unnumbered item
    /// doesn't count (ODF's list header, LibreOffice's unnumbered entry).
    bool numbered{true};
    /// The number this item shows, if it restarts the count; later items
    /// count on from it.
    std::optional<int> start_value;

    bool operator==(const list_item &) const = default;
};

class list : public styled<list, style_family::list>
{
public:
    list() = default;
    /// Items, or content that makes one each (list("a", "b")).
    template <typename... Args>
        requires(sizeof...(Args) >= 1) &&
        ((std::convertible_to<Args, list_item> && !std::is_same_v<std::remove_cvref_t<Args>, list>) &&
            ...)
    list(Args &&...items)
    {
        (add(std::forward<Args>(items)), ...);
    }

    list &add(list_item it)
    {
        m_items.push_back(std::move(it));
        return *this;
    }

    std::vector<list_item> &items() { return m_items; }
    const std::vector<list_item> &items() const { return m_items; }

    /// Whether the numbering carries on from the last list before it with
    /// the same list style (at the same depth), rather than starting again.
    bool continue_numbering{false};

    bool operator==(const list &) const = default;

private:
    std::vector<list_item> m_items;
};

} // namespace docsmith
