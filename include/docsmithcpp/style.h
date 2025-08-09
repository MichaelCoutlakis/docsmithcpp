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

#include "docsmithcpp/named_registry.h"

namespace docsmith
{

enum class align_horiz
{
    left,
    centre,
    right,
};

enum class align_vert
{
    top,
    centre,
    bottom,
};

class list;

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

struct font_name
{
public:
    explicit font_name(const std::string &font_name) :
        m_font_name{font_name}
    {
    }
    const std::string &get_name() const { return m_font_name; }

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
    float m_points;
};

struct dim
{
    double m_num;
    std::string m_unit;
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
    std::optional<font_size> m_font_size;
    std::optional<font_style> m_font_style;
    std::optional<font_name> m_font_name;
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
    std::optional<align_horiz> m_horiz_pos;
    std::optional<align_vert> m_vert_pos;
};

struct paragraph_props
{
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
    style_name m_name;
    style_name m_parent_style;
    std::optional<graphics_props> m_graphics_props;
    std::optional<text_props> m_text_props;
    std::optional<paragraph_props> m_paragraph_props;
};

inline constexpr std::string get_style_name(const style &s) { return s.m_name.get_name(); }

using style_registry = named_registry<style, get_style_name>;

template <typename Derived>
class styled
{
public:
    styled() = default;
    explicit styled(style_name sn) :
        m_style_name(std::move(sn))
    {
    }
    Derived &set_style(style_name sn)
    {
        m_style_name = std::move(sn);
        return static_cast<Derived &>(*this);
    }

    template <typename HasStyle>
    Derived &set_style(const styled<HasStyle> &has_style)
    {
        return set_style(style_name(has_style.get_style()));
    }
    const style_name &get_style() const { return m_style_name; }

private:
    style_name m_style_name;
};

};