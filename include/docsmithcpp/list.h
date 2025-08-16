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
#include <string>
#include <variant>

#include "docsmithcpp/element.h"
#include "docsmithcpp/nodes.h"
#include "docsmithcpp/style.h"
#include "docsmithcpp/text.h"

#include "docsmithcpp/forward_decl.h"
#include "docsmithcpp/named_registry.h"

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
    constexpr static inline std::string bullet() { return "•"; }
    constexpr static inline std::string black_circ() { return "●"; }
    constexpr static inline std::string heavy_check_mark() { return "✔"; }
    constexpr static inline std::string ballot_x() { return "✗"; }
    constexpr static inline std::string right_arrow() { return "➔"; }
    constexpr static inline std::string three_d_right_arrow() { return "➢"; }
};

class list_style_bullet
{
public:
    list_style_bullet(const style &assoc_style, int level, std::string bullet_char) :
        m_style_name(assoc_style.m_name.get_name()), m_level(level), m_bullet_char(bullet_char)
    {
    }
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
    style_name m_name;
    std::vector<level_style> m_level_styles;
};

constexpr std::string get_list_style_name(const list_style &ls) { return ls.m_name.get_name(); }
using list_style_registry = named_registry<list_style, get_list_style_name>;

// clang-format off
template <> struct is_valid_child<list_style, list_style_bullet> : std::true_type {};
template <> struct is_valid_child<list_style, list_style_num> : std::true_type {};
// clang-format on

class list_item : public element_base<list_item>,
                  public nodes<list_item>,
                  public elem_tagged<paragraph, elem_t::lit>
{
public:
    using nodes::nodes;

    /// List items don't contain raw text - must always be wrapped in a paragraph. This wrapping is
    /// provided here for convenience.
    list_item(std::string t);

    bool operator==(const list_item &other) const
    {
        return compare_equality(m_children, other.m_children);
    }
};

// clang-format off
template <> struct is_valid_child<list_item, paragraph> : std::true_type {};
template <> struct is_valid_child<list_item, heading> : std::true_type {};
template <> struct is_valid_child<list_item, list> : std::true_type {};
// clang-format on

class list : public element_base<list>,
             public nodes<list>,
             public styled<list>,
             public elem_tagged<paragraph, elem_t::lst>
{
public:
    using nodes::nodes;

    bool operator==(const list &other) const
    {
        return compare_equality(m_children, other.m_children);
    }
};

// clang-format off
template <> struct is_valid_child<list, list_item> : std::true_type {};

// clang-format on
}
