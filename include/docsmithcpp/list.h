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
#include <string>
#include <variant>
#include <vector>

#include "docsmithcpp/named_registry.h"
#include "docsmithcpp/nodes_fwd.h"
#include "docsmithcpp/style.h"

namespace docsmith
{

enum class list_enum : char
{
    arabic = '1',
    lower_alpha = 'a',
    upper_alpha = 'A',
    lower_roman = 'i',
    upper_roman = 'I',
};

/// Represents the formatting properties for a specific list level
class list_level_props
{
public:
    bool operator==(const list_level_props &) const = default;
};

/// Represents a text:list-level-style-number
class list_style_num
{
public:
    list_style_num(const style &s, int level, list_enum format_, std::string suffix = ".",
        int start_from = 1, std::string prefix = {}) :
        m_level(level),
        m_format(format_),
        m_num_prefix(prefix),
        m_num_suffix(suffix),
        m_start_from(start_from),
        m_style_name(s.m_name.get_name())
    {
    }
    bool operator==(const list_style_num &) const = default;

    int m_level;                            //!< Nesting level to which this list style applies
    list_enum m_format;                     //!< Numbering format
    std::string m_num_prefix;               //!< Place before the number
    std::string m_num_suffix{"."};          //!< Place after the number
    int m_start_from{1};                    //!< Start numbering from here
    style_name m_style_name;                //!< Style of the number / bullet text
    std::optional<text_props> m_text_props; //!<
    std::optional<list_level_props> m_level_props; //!< List level properties
};

struct bullet_type
{
    // Not constexpr: std::string only became a literal type in GCC 12's
    // libstdc++, and GCC 11 is supported.
    static std::string bullet() { return "•"; }
    static std::string black_circ() { return "●"; }
    static std::string heavy_check_mark() { return "✔"; }
    static std::string ballot_x() { return "✗"; }
    static std::string right_arrow() { return "➔"; }
    static std::string three_d_right_arrow() { return "➢"; }
};

class list_style_bullet
{
public:
    list_style_bullet(const style &assoc_style, int level, std::string bullet_char) :
        m_style_name(assoc_style.m_name.get_name()), m_level(level), m_bullet_char(bullet_char)
    {
    }
    bool operator==(const list_style_bullet &) const = default;

    style_name m_style_name;
    int m_level; //!< Nesting level to which this list style applies
    std::string m_bullet_char;
};

class list_style
{
public:
    using level_style = std::variant<list_style_num, list_style_bullet>;
    list_style(style_name sn, std::initializer_list<level_style> level_styles) :
        m_name(sn), m_level_styles(level_styles)
    {
    }
    operator style_name() const { return m_name; }
    bool operator==(const list_style &) const = default;

    style_name m_name;
    std::vector<level_style> m_level_styles;
};

inline std::string get_list_style_name(const list_style &ls) { return ls.m_name.get_name(); }
using list_style_registry = named_registry<list_style, get_list_style_name>;

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

class list : public styled<list>
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
