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

class style_name
{
public:
    style_name(const std::string &name = {}) :
        m_name{name}
    {
    }
    const std::string &GetName() const { return m_name; }

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

class span : public element_base<span>
{
public:
    explicit span(const std::string &text, const style_name &name = style_name{}) :
        m_text{text}, m_style_name{name}
    {
    }

    explicit span(const char *text) :
        m_text(text)
    {
    }

    span &set(const style_name &name)
    {
        m_style_name = name;
        return *this;
    }
    std::string get_text() const { return m_text; }

    bool operator==(const span &rhs) const
    {
        bool b1 = m_style_name == rhs.m_style_name;
        bool b2 = m_text == rhs.m_text;
        return b1 && b2;
    }

private:
    style_name m_style_name;
    std::string m_text;
};

class list_item
{
public:
    bool operator==(const list_item &rhs) const = default;
};

class list
{
public:
    enum class Marker
    {
        DecimalNum,
        LowerAlpha,
        UpperAlpha,
        LowerRoman,
        UpperRoman,
        Bullet
    };

    template <typename... Args>
    list(Marker marker, int start, Args &&...args) :
        m_marker(marker), m_start(start)
    {
        (add(std::move(args)), ...);
    }

    bool IsOrdered() const { return m_marker < Marker::Bullet; }

    int GetStart() const { return m_start; }

    bool operator==(const list &rhs) const = default;

private:
    Marker m_marker{Marker::Bullet};
    int m_start{1};
};

class hyperlink
{
public:
    template <typename... Args>
    hyperlink(std::string URL, Args... args) :
        m_URL(URL)
    {
    }

    std::string GetURL() const { return m_URL; }

    bool operator==(const hyperlink &rhs) const = default;

private:
    std::string m_URL;
};

class Figure
{
public:
    explicit Figure(std::string filename) :
        m_filename(filename)
    {
    }

    std::string GetFileName() const { return m_filename; }
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
