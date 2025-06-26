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

namespace docsmith
{

class list;

class style_name
{
public:
    explicit style_name(const std::string &name = {}) :
        m_name{name}
    {
    }
    const std::string &get_name() const { return m_name; }

    bool operator==(const style_name &rhs) const = default;

private:
    std::string m_name;
};

struct font_name
{
public:
    explicit font_name(const std::string &font_name) :
        m_font_name{font_name}
    {
    }

private:
    std::string m_font_name;
};

enum class font_style
{
    Normal,
    Italic,
    Oblique
};
enum class FontWeight
{
    Normal,
    Bold
};

struct font_size
{
    explicit font_size(float points) :
        m_points{points}
    {
    }
    float m_points;
};

class Style
{
public:
    template <typename... Args>
    Style(const style_name &name, Args... args) :
        m_style_name{name}
    {
        (Set(std::move(args)), ...);
    }

    Style &Set(const style_name &name)
    {
        m_style_name = name;
        return *this;
    }
    Style &Set(const font_name &font_name)
    {
        m_font_name = font_name;
        return *this;
    }
    Style &Set(const font_size &font_size)
    {
        m_font_size = font_size;
        return *this;
    }
    Style &Set(const FontWeight &font_weight)
    {
        m_font_weight = font_weight;
        return *this;
    }

    style_name m_style_name;        //!< The style name for the purpose of identification and lookup
    std::string m_display_name{""}; //!< Human friendly display name

    font_name m_font_name{"Arial"};
    font_size m_font_size{12.f};
    font_style m_font_style{font_style::Normal};
    FontWeight m_font_weight{FontWeight::Normal};
};

class text : public element_base<text>
{
public:
    explicit text(const char *s) :
        m_text(s)
    {
    }

    explicit text(const std::string &s) :
        m_text(s)
    {
    }

    bool operator==(const text &other) const { return m_text == other.m_text; }

    std::string m_text;
};

class span : public element_base<span>, public element_children<span>
{
public:
    template <typename... Args,
        typename = std::enable_if_t<(is_valid_child_v<span, std::decay_t<Args>> && ...)>>
    explicit span(Args &&...args) :
        element_children<span>(std::forward<Args>(args)...)
    {
    }

    template <typename... Args,
        typename = std::enable_if_t<(is_valid_child_v<span, std::decay_t<Args>> && ...)>>
    explicit span(const style_name &sn, Args &&...args) :
        element_children<span>(std::forward<Args>(args)...), m_style_name(sn)
    {
    }

    span &set(const style_name &name)
    {
        m_style_name = name;
        return *this;
    }

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
    enum class num_format : char
    {
        arabic = '1',
        lower_alpha = 'a',
        upper_alpha = 'A',
        lower_roman = 'i',
        upper_roman = 'I',
    };

    list_style_num(
        num_format format, std::string suffix = ".", int start_from = 1, std::string prefix = {}) :
        m_format(format), m_num_prefix(prefix), m_num_suffix(suffix), m_start_from(start_from)
    {
    }
    num_format m_format;           //!< Numbering format
    std::string m_num_prefix;      //!< Place before the number
    std::string m_num_suffix{"."}; //!< Place after the number
    int m_start_from{1};           //!< Start numbering from here
};

class list_style_bullet
{
public:
    struct bullets
    {
        constexpr static inline std::string bullet() { return "•"; }
        constexpr static inline std::string black_circ() { return "●"; }
        constexpr static inline std::string heavy_check_mark() { return "✔"; }
        constexpr static inline std::string ballot_x() { return "✗"; }
        constexpr static inline std::string right_arrow() { return "➔"; }
        constexpr static inline std::string three_d_right_arrow() { return "➢"; }
    };
    list_style_bullet(std::string bullet_char) :
        m_bullet_char(bullet_char)
    {
    }
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
    template <typename... Args>
    list_item(Args &&...args) :
        element_children<list_item>(std::forward<Args>(args)...)
    {
    }
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

class list : public element_base<list>, public element_children<list>
{
public:
    template <typename... Args>
    list(style_name style_name_, Args &&...args) :
        element_children<list>(std::forward<Args>(args)...), m_style_name(style_name_)
    {
    }
    style_name m_style_name;

    bool operator==(const list &other) const { return m_style_name == other.m_style_name; }
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

// clang-format on

class Figure
{
public:
    explicit Figure(std::string filename) :
        m_filename(filename)
    {
    }

    std::string get_filename() const { return m_filename; }
    bool operator==(const Figure &rhs) const
    {
        bool b = m_filename == rhs.m_filename;
        return b;
    }

private:
    std::string m_filename;
};

class paragraph : public element_base<paragraph>, public element_children<paragraph>
{
public:
    paragraph() = default;

    template <typename... Args>
    paragraph(Args &&...args) :
        element_children<paragraph>(std::forward<Args>(args)...)
    {
    }

    bool operator==(const paragraph &other) const
    {
        return compare_equality(m_children, other.m_children);
    }

private:
};

// clang-format off
template <> struct is_valid_child<paragraph, span> : std::true_type {};
template <> struct is_valid_child<paragraph, text> : std::true_type {};
// clang-format on

class heading : public element_base<heading>, public element_children<heading>
{
public:
    template <typename... Args>
    heading(int level, Args... args) :
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
// clang-format on
}
