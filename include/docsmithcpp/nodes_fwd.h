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

// Shared foundation for the node headers: the recursive-variant forward
// declarations, the content-conversion helpers, and the has_children mixin.
// Every per-node header (inline_nodes.h, list.h, table.h, nodes.h) includes
// this; the variants themselves are defined later, once their alternatives are
// complete.

#include <concepts>
#include <string>
#include <type_traits>
#include <vector>

#include "docsmithcpp/box.h"
#include "docsmithcpp/style.h"

namespace docsmith
{

// The two node variants are wrapped in a struct so a std::vector of the (still
// incomplete) variant can appear as a member of the node types below.
struct inline_node;
struct block_node;

// Normalise anything usable as content (a node, a leaf, or a bare string) into
// the right node variant. Declared here; defined once the variants are complete.
inline_node to_inline(const char *s);
inline_node to_inline(const std::string &s);
inline_node to_inline(inline_node n);
template <typename T>
inline_node to_inline(T leaf);

block_node to_block(const char *s);
block_node to_block(const std::string &s);
block_node to_block(block_node n);
template <typename T>
block_node to_block(T node);

// The node types, for the content concepts below.
struct text;
struct bookmark;
class frame;
class span;
class hyperlink;
struct field;
struct unknown;
class paragraph;
class heading;
class list;
class table;
struct page_break;
struct table_of_contents;

template <typename T, typename... Ts>
concept one_of = (std::is_same_v<std::remove_cvref_t<T>, Ts> || ...);

/// What a paragraph, heading, span or hyperlink takes as a child: an inline
/// node, or a string, which is text.
template <typename T>
concept inline_content =
    one_of<T, inline_node, text, span, hyperlink, frame, bookmark, field, unknown> ||
    std::convertible_to<T, std::string>;

/// What a document, list item or table cell takes as a child: a block node,
/// or a string, which is a paragraph.
template <typename T>
concept block_content =
    one_of<T, block_node, paragraph, heading, list, table, frame, page_break, table_of_contents,
        unknown> ||
    std::convertible_to<T, std::string>;

/// T is content for a sequence of Child (inline_node or block_node).
template <typename T, typename Child>
concept content_for = (std::is_same_v<Child, inline_node> && inline_content<T>) ||
    (std::is_same_v<Child, block_node> && block_content<T>);

/// Arguments for a variadic content constructor of Self: one or more, each
/// content for Child and none of them a Self -- so a single same-type argument
/// still selects the copy/move constructor (nesting a same-type child uses
/// .add()).
template <typename Self, typename Child, typename... Args>
concept content_args = (sizeof...(Args) >= 1) &&
    ((content_for<Args, Child> && !std::is_same_v<std::remove_cvref_t<Args>, Self>) && ...);

/// CRTP mixin: a node's child sequence, plus children()/add()/iteration.
/// When Boxed is true the storage is routed through box<> to break the variant
/// recursion (see box.h); the box is hidden behind children() so callers never
/// see it and the boxing choice stays an implementation detail.
template <typename Child, typename Derived, bool Boxed>
class has_children
{
public:
    std::vector<Child> &children()
    {
        if constexpr(Boxed)
            return *m_children;
        else
            return m_children;
    }
    const std::vector<Child> &children() const
    {
        if constexpr(Boxed)
            return *m_children;
        else
            return m_children;
    }

    auto begin() { return children().begin(); }
    auto end() { return children().end(); }
    auto begin() const { return children().begin(); }
    auto end() const { return children().end(); }

    template <content_for<Child> T>
    Derived &add(T &&content)
    {
        if constexpr(std::is_same_v<Child, inline_node>)
            children().push_back(to_inline(std::forward<T>(content)));
        else
            children().push_back(to_block(std::forward<T>(content)));
        return static_cast<Derived &>(*this);
    }

    bool operator==(const has_children &) const = default;

private:
    std::conditional_t<Boxed, box<std::vector<Child>>, std::vector<Child>> m_children;
};

} // namespace docsmith
