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
#include <cstdint>
#include <optional>
#include <string>

#include "docsmithcpp/named_registry.h"

namespace docsmith
{

/// style:horizontal-pos
enum class align_horiz
{
    left,
    centre,
    right,
    from_inside,
    from_left,
    inside,
    outside
};

/// style:vertical-pos
enum class align_vert
{
    top,
    middle,
    bottom,
    from_top,
    below
};

class style_name
{
public:
    style_name() = default;

    style_name(const char *name) :
        style_name(std::string(name))
    {
    }
    style_name(const std::string &name) :
        m_name{name}
    {
    }

    const std::string &get_name() const { return m_name; }
    bool is_empty() const { return m_name.empty(); }
    bool operator==(const style_name &rhs) const = default;

private:
    std::string m_name;
};

/// Style names writers know without a definition in text_doc::styles(). Each
/// writer maps them to its format's own style of that name.
namespace builtin
{
inline const style_name title{"Title"};
}

struct font_name
{
public:
    explicit font_name(const std::string &font_name) :
        m_font_name{font_name}
    {
    }
    const std::string &get_name() const { return m_font_name; }
    bool operator==(const font_name &rhs) const = default;

private:
    std::string m_font_name;
};

enum class font_style
{
    normal,
    italic,
    oblique
};
enum class font_weight
{
    normal,
    bold
};

struct font_size
{
    explicit font_size(float points) :
        m_points{points}
    {
    }
    bool operator==(const font_size &rhs) const = default;
    float m_points;
};

/// An sRGB text colour.
struct colour
{
    static constexpr colour rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b)
    { return {r, g, b}; }
    bool operator==(const colour &rhs) const = default;

    std::uint8_t m_r{};
    std::uint8_t m_g{};
    std::uint8_t m_b{};
};

struct dim
{
    double m_num;
    std::string m_unit;
    bool operator==(const dim &rhs) const = default;
};

struct text_props
{
    template <typename... Args>
    explicit text_props(Args &&...args)
    {
        (set(args), ...);
    }

    text_props &set(const font_size &fs)
    {
        m_font_size = fs;
        return *this;
    }
    text_props &set(const font_style &fs)
    {
        m_font_style = fs;
        return *this;
    }
    text_props &set(const font_name &fn)
    {
        m_font_name = fn;
        return *this;
    }
    text_props &set(const font_weight &fw)
    {
        m_font_weight = fw;
        return *this;
    }
    text_props &set(const colour &c)
    {
        m_colour = c;
        return *this;
    }
    bool operator==(const text_props &rhs) const = default;

    std::optional<font_size> m_font_size;
    std::optional<font_style> m_font_style;
    std::optional<font_name> m_font_name;
    std::optional<font_weight> m_font_weight;
    std::optional<colour> m_colour;
};

/// fo:text-align
enum class text_align
{
    start,
    end,
    left,
    right,
    centre,
    justify
};

enum class break_type
{
    column,
    page,
    even_page,
    odd_page,
    automatic
};

// clang-format off
struct break_before { break_type m_break_type; bool operator==(const break_before&) const = default; };
struct break_after { break_type m_break_type; bool operator==(const break_after&) const = default; };
// clang-format on

class paragraph_props
{
public:
    template <typename... Args>
    explicit paragraph_props(Args &&...args)
    {
        (set(std::forward<Args>(args)), ...);
    }

    paragraph_props &set(const break_before &b)
    {
        m_break_before = b;
        return *this;
    }
    paragraph_props &set(const break_after &b)
    {
        m_break_after = b;
        return *this;
    }
    bool operator==(const paragraph_props &rhs) const = default;

    std::optional<break_before> m_break_before;
    std::optional<break_after> m_break_after;
};

class graphics_props
{
public:
    template <typename... Args>
    explicit graphics_props(Args &&...args)
    {
        (set(args), ...);
    }

    graphics_props &set(align_horiz h)
    {
        m_horiz_pos = h;
        return *this;
    }
    graphics_props &set(align_vert v)
    {
        m_vert_pos = v;
        return *this;
    }
    bool operator==(const graphics_props &rhs) const = default;

    std::optional<align_horiz> m_horiz_pos;
    std::optional<align_vert> m_vert_pos;
};

/// What kind of node a style applies to. ODF and DOCX keep styles of different
/// families apart, so the same name may be defined once per family.
enum class style_family
{
    paragraph, //!< paragraph, heading
    text,      //!< span
    graphic,   //!< frame
    table,
    table_column,
    table_row,
    table_cell
};

///
class style
{
public:
    style() = default;
    template <typename... Args>
    explicit style(const style_name &name, const Args &...args) :
        m_name{name}
    {
        (set(args), ...);
    }

    style &set(style_family f)
    {
        m_family = f;
        return *this;
    }
    style &set(const graphics_props &props)
    {
        m_graphics_props = props;
        return *this;
    }
    style &set(const text_props &props)
    {
        m_text_props = props;
        return *this;
    }
    style &set(const paragraph_props &props)
    {
        m_paragraph_props = props;
        return *this;
    }
    operator style_name() const { return m_name; }
    bool operator==(const style &rhs) const = default;

    style_name m_name;
    /// Empty: writers that need a family work it out from the nodes that use
    /// the style, writing it once per family if they differ.
    std::optional<style_family> m_family;
    style_name m_parent_style;
    std::optional<graphics_props> m_graphics_props;
    std::optional<text_props> m_text_props;
    std::optional<paragraph_props> m_paragraph_props;
};

inline std::string get_style_name(const style &s) { return s.m_name.get_name(); }

using style_registry = named_registry<style, get_style_name>;

/// CRTP mixin giving a node a style name plus a fluent set_style(). It is a
/// plain code-sharing base (no virtuals, no shared root), not the polymorphic
/// plumbing the old design used. A derived type's defaulted operator== compares
/// this base subobject, so style equality composes automatically.
template <typename Derived>
struct styled
{
    style_name style;

    Derived &set_style(style_name sn)
    {
        style = std::move(sn);
        return static_cast<Derived &>(*this);
    }

    bool operator==(const styled &) const = default;
};

} // namespace docsmith
