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
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "docsmithcpp/element.h"
#include "docsmithcpp/has_children.h"
#include "docsmithcpp/style.h"
#include "docsmithcpp/text.h"

namespace docsmith
{

class text;
class span;
class heading;
class paragraph;
class hyperlink;
class text_doc;
class list;
class list_item;
class frame;
class image;

class style_graphics
{
public:
    template <typename... Args>
    explicit style_graphics(Args &&...args)
    {
        (set(args), ...);
    }

    style_graphics &set(align_horiz h)
    {
        m_horiz_pos = h;
        return *this;
    }
    style_graphics &set(align_vert v)
    {
        m_vert_pos = v;
        return *this;
    }
    std::optional<align_horiz> m_horiz_pos;
    std::optional<align_vert> m_vert_pos;
};

class style
{
public:
    template <typename... Args>
    explicit style(const style_name &name, const Args &...args) :
        m_name{name}
    {
        (set(args), ...);
    }

    style &set(const style_graphics &g)
    {
        m_graphics_props = g;
        return *this;
    }
    operator style_name() const { return m_name; }
    style_name m_name;
    std::optional<style_graphics> m_graphics_props;
};

class span : public element_base<span>, public element_children<span>, public styled<span>
{
public:
    using element_children::element_children;

    bool operator==(const span &rhs) const
    {
        return m_style_name == rhs.m_style_name && compare_equality(m_children, rhs.m_children);
    }

private:
    style_name m_style_name;
};

// clang-format off
template <> struct is_valid_child<span, span> : std::true_type {};
template <> struct is_valid_child<span, text> : std::true_type {};
// clang-format on

class list_style_num
{
public:
    enum class format : char
    {
        arabic = '1',
        lower_alpha = 'a',
        upper_alpha = 'A',
        lower_roman = 'i',
        upper_roman = 'I',
    };

    list_style_num(style_name sn, format format_, std::string suffix = ".", int start_from = 1,
        std::string prefix = {}) :
        m_style_name(sn),
        m_format(format_),
        m_num_prefix(prefix),
        m_num_suffix(suffix),
        m_start_from(start_from)
    {
    }
    style_name m_style_name;       //!< Style name
    format m_format;               //!< Numbering format
    std::string m_num_prefix;      //!< Place before the number
    std::string m_num_suffix{"."}; //!< Place after the number
    int m_start_from{1};           //!< Start numbering from here
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
    list_style_bullet(style_name sn, std::string bullet_char) :
        m_style_name(sn), m_bullet_char(bullet_char)
    {
    }
    style_name m_style_name;
    std::string m_bullet_char;
};

class list_style
{
public:
    // list_style(

    style_name m_style_name;

private:
};

class list_item : public element_base<list_item>, public element_children<list_item>
{
public:
    using element_children::element_children;
    bool operator==(const list_item &other) const
    {
        return compare_equality(m_children, other.m_children);
    }
};

// clang-format off
template <> struct is_valid_child<list_item, paragraph> : std::true_type {};
template <> struct is_valid_child<list_item, heading> : std::true_type {};
template <> struct is_valid_child<list_item, list> : std::true_type {};
template <> struct is_valid_child<list_item, text> : std::true_type {};
// clang-format on

class list : public element_base<list>, public element_children<list>, public styled<list>
{
public:
    using element_children::element_children;

    bool operator==(const list &other) const
    {
        return compare_equality(m_children, other.m_children);
    }
};

// clang-format off
template <> struct is_valid_child<list, list_item> : std::true_type {};

// clang-format on

class hyperlink : public element_base<hyperlink>, public element_children<hyperlink>
{
public:
    template <typename... Args>
    hyperlink(std::string url, Args &&...args) :
        element_children<hyperlink>(std::forward<Args>(args)...), m_url(url)
    {
    }

    std::string get_url() const { return m_url; }

    bool operator==(const hyperlink &rhs) const
    {
        return m_url == rhs.m_url && compare_equality(m_children, rhs.m_children);
    }

private:
    std::string m_url;
};

// clang-format off
template <> struct is_valid_child<hyperlink, span> : std::true_type {};
template <> struct is_valid_child<hyperlink, text> : std::true_type {};

// clang-format on

class image : public element_base<image>
{
public:
    explicit image(std::string uri) :
        m_uri(uri)
    {
    }

    std::string get_uri() const { return m_uri; }

    bool operator==(const image &rhs) const
    {
        bool b = m_uri == rhs.m_uri;
        return b;
    }

private:
    std::string m_uri;
};

class frame : public element_base<frame>, public element_children<frame>, public styled<frame>
{
public:
    using element_children::element_children;

    bool operator==(const frame &other) const
    {
        return get_style() == other.get_style() && compare_equality(m_children, other.m_children);
    }
};

// clang-format off
template <> struct is_valid_child<frame, image> : std::true_type {};

// clang-format on

class paragraph : public element_base<paragraph>,
                  public element_children<paragraph>,
                  public styled<paragraph>
{
public:
    using element_children::element_children;

    bool operator==(const paragraph &other) const
    {
        return get_style() == other.get_style() && compare_equality(m_children, other.m_children);
    }

private:
};

// clang-format off
template <> struct is_valid_child<paragraph, span> : std::true_type {};
template <> struct is_valid_child<paragraph, text> : std::true_type {};
template <> struct is_valid_child<paragraph, hyperlink> : std::true_type {};
template <> struct is_valid_child<paragraph, frame> : std::true_type {};
// clang-format on

class heading : public element_base<heading>,
                public element_children<heading>,
                public styled<heading>
{
public:
    template <typename... Args>
    heading(int level, Args &&...args) :
        element_children<heading>(std::forward<Args>(args)...), m_level(level)
    {
    }

    int level() const { return m_level; }

    bool operator==(const heading &other) const
    {
        return m_level == other.m_level && compare_equality(this->m_children, other.m_children);
    }

private:
    int m_level{1};
};

// clang-format off
template <> struct is_valid_child<heading, paragraph> : std::true_type {};
template <> struct is_valid_child<heading, span> : std::true_type {};
template <> struct is_valid_child<heading, text> : std::true_type {};
// clang-format on

class text_doc : public element_base<text_doc>, public element_children<text_doc>
{
public:
    text_doc() = default;

    template <typename... Args>
    text_doc(Args &&...args) :
        element_children<text_doc>(std::forward<Args>(args)...)
    {
    }

    bool operator==(const text_doc &other) const
    {
        return compare_equality(m_children, other.m_children);
    }
};

// clang-format off
template <> struct is_valid_child<text_doc, heading> : std::true_type {};
template <> struct is_valid_child<text_doc, paragraph> : std::true_type {};
template <> struct is_valid_child<text_doc, list> : std::true_type {};
template <> struct is_valid_child<text_doc, frame> : std::true_type {};
// clang-format on
} // namespace docsmith
