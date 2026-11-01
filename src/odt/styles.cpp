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
#include "odt/styles.h"

#include <charconv>
#include <string>
#include <string_view>
#include <utility>

#include "format.h"
#include "odf/package.h"

namespace docsmith::odt
{
namespace
{
// Each enum's ODF spelling, in one table per enum so reading and writing agree.
template <typename E>
struct spelling
{
    E value;
    const char *odf;
};

constexpr spelling<align_horiz> horiz_names[] = {{align_horiz::left, "left"},
    {align_horiz::centre, "center"},
    {align_horiz::right, "right"},
    {align_horiz::from_inside, "from-inside"},
    {align_horiz::from_left, "from-left"},
    {align_horiz::inside, "inside"},
    {align_horiz::outside, "outside"}};
constexpr spelling<align_vert> vert_names[] = {{align_vert::top, "top"},
    {align_vert::middle, "middle"},
    {align_vert::bottom, "bottom"},
    {align_vert::from_top, "from-top"},
    {align_vert::below, "below"}};
constexpr spelling<font_style> font_style_names[] = {{font_style::normal, "normal"},
    {font_style::italic, "italic"},
    {font_style::oblique, "oblique"}};
constexpr spelling<break_type> break_names[] = {{break_type::automatic, "auto"},
    {break_type::column, "column"},
    {break_type::page, "page"},
    {break_type::even_page, "even-page"},
    {break_type::odd_page, "odd-page"}};
constexpr spelling<style_family> family_names[] = {{style_family::paragraph, "paragraph"},
    {style_family::text, "text"},
    {style_family::graphic, "graphic"},
    {style_family::table, "table"},
    {style_family::table_column, "table-column"},
    {style_family::table_row, "table-row"},
    {style_family::table_cell, "table-cell"}};

template <typename E, std::size_t N>
const char *to_odf(const spelling<E> (&names)[N], E v)
{
    for(const auto &s : names)
        if(s.value == v)
            return s.odf;
    return "";
}

template <typename E, std::size_t N>
std::optional<E> from_odf(const spelling<E> (&names)[N], std::string_view odf)
{
    for(const auto &s : names)
        if(odf == s.odf)
            return s.value;
    return std::nullopt;
}

const char *to_odf(font_weight fw) { return fw == font_weight::bold ? "bold" : "normal"; }

/// "bold" / "normal", or a CSS-style number (600 and up is bold).
std::optional<font_weight> font_weight_from(std::string_view odf)
{
    if(odf == "bold")
        return font_weight::bold;
    if(odf == "normal")
        return font_weight::normal;
    int n{};
    if(auto [p, ec] = std::from_chars(odf.data(), odf.data() + odf.size(), n);
        ec == std::errc{} && p == odf.data() + odf.size())
        return n >= 600 ? font_weight::bold : font_weight::normal;
    return std::nullopt;
}

/// Points from e.g. "12pt"; other units (and percentages) aren't modelled.
std::optional<font_size> font_size_from(std::string_view odf)
{
    if(!odf.ends_with("pt"))
        return std::nullopt;
    odf.remove_suffix(2);
    float pt{};
    if(auto [p, ec] = std::from_chars(odf.data(), odf.data() + odf.size(), pt);
        ec != std::errc{} || p != odf.data() + odf.size())
        return std::nullopt;
    return font_size(pt);
}

/// "#rrggbb"
std::optional<colour> colour_from(std::string_view odf)
{
    if(odf.size() != 7 || odf.front() != '#')
        return std::nullopt;
    std::uint8_t c[3]{};
    for(int i = 0; i != 3; ++i)
        if(auto [p, ec] = std::from_chars(odf.data() + 1 + 2 * i, odf.data() + 3 + 2 * i, c[i], 16);
            ec != std::errc{} || p != odf.data() + 3 + 2 * i)
            return std::nullopt;
    return colour::rgb(c[0], c[1], c[2]);
}

std::optional<list_enum> list_enum_from(std::string_view odf)
{
    for(list_enum e : {list_enum::arabic,
            list_enum::lower_alpha,
            list_enum::upper_alpha,
            list_enum::lower_roman,
            list_enum::upper_roman})
        if(odf.size() == 1 && odf.front() == static_cast<char>(e))
            return e;
    return std::nullopt;
}

std::optional<text_props> read_text_props(const pugi::xml_node &n)
{
    if(!n)
        return std::nullopt;
    text_props tp;
    if(auto fs = font_size_from(n.attribute("fo:font-size").as_string()))
        tp.set(*fs);
    if(auto fs = from_odf(font_style_names, n.attribute("fo:font-style").as_string()))
        tp.set(*fs);
    if(auto a = n.attribute("style:font-name"))
        tp.set(font_name(a.as_string()));
    if(auto fw = font_weight_from(n.attribute("fo:font-weight").as_string()))
        tp.set(*fw);
    if(auto c = colour_from(n.attribute("fo:color").as_string()))
        tp.set(*c);
    return tp == text_props{} ? std::nullopt : std::optional(std::move(tp));
}

std::optional<paragraph_props> read_paragraph_props(const pugi::xml_node &n)
{
    if(!n)
        return std::nullopt;
    paragraph_props pp;
    if(auto b = from_odf(break_names, n.attribute("fo:break-before").as_string()))
        pp.set(break_before{*b});
    if(auto b = from_odf(break_names, n.attribute("fo:break-after").as_string()))
        pp.set(break_after{*b});
    return pp == paragraph_props{} ? std::nullopt : std::optional(std::move(pp));
}

std::optional<graphics_props> read_graphics_props(const pugi::xml_node &n)
{
    if(!n)
        return std::nullopt;
    graphics_props gp;
    if(auto h = from_odf(horiz_names, n.attribute("style:horizontal-pos").as_string()))
        gp.set(*h);
    if(auto v = from_odf(vert_names, n.attribute("style:vertical-pos").as_string()))
        gp.set(*v);
    return gp == graphics_props{} ? std::nullopt : std::optional(std::move(gp));
}

void read_into(const pugi::xml_node &container, text_doc &doc)
{
    for(auto c : container.children())
    {
        const std::string_view tag = c.name();
        if(tag == "style:style")
        {
            if(auto s = read_style(c); s && !doc.styles().find(s->m_name.get_name()))
                doc.styles().add(*s);
        }
        else if(tag == "text:list-style")
        {
            if(!doc.list_styles().find(c.attribute("style:name").as_string()))
                doc.list_styles().add(read_list_style(c));
        }
    }
}
} // namespace

void write_xml(pugi::xml_node &n, const style_name &sn)
{ n.append_attribute("style:name").set_value(sn.get_name().c_str()); }

void write_xml(pugi::xml_node &n, const style &s)
{ write_xml(n, s, s.m_family.value_or(style_family::paragraph)); }

void write_xml(pugi::xml_node &n, const style &s, style_family family)
{
    auto style_node = n.append_child("style:style");
    write_xml(style_node, s.m_name);
    style_node.append_attribute("style:family") = to_odf(family_names, family);

    if(!s.m_parent_style.is_empty())
        style_node.append_attribute("style:parent-style-name") =
            s.m_parent_style.get_name().c_str();

    if(s.m_graphics_props)
        write_xml(style_node, s.m_graphics_props.value());

    if(s.m_paragraph_props)
        write_xml(style_node, s.m_paragraph_props.value());

    if(s.m_text_props)
        write_xml(style_node, s.m_text_props.value());
}

void write_xml(pugi::xml_node &n, const list_style &ls)
{
    auto ls_node = n.append_child("text:list-style");
    write_xml(ls_node, ls.m_name);

    for(const auto &level_style : ls.m_level_styles)
        std::visit([&](const auto &s) { write_xml(ls_node, s); }, level_style);
}

void write_xml(pugi::xml_node &n, const list_style_bullet &lsb)
{
    auto bullet_node = n.append_child("text:list-level-style-bullet");
    bullet_node.append_attribute("text:level") = lsb.m_level;
    if(!lsb.m_style_name.is_empty())
        bullet_node.append_attribute("text:style-name") = lsb.m_style_name.get_name().c_str();
    bullet_node.append_attribute("text:bullet-char") = lsb.m_bullet_char.c_str();
}

void write_xml(pugi::xml_node &n, const list_style_num &s)
{
    auto num_node = n.append_child("text:list-level-style-number");
    num_node.append_attribute("text:level") = s.m_level;
    if(!s.m_style_name.is_empty())
        num_node.append_attribute("text:style-name") = s.m_style_name.get_name().c_str();

    num_node.append_attribute("loext:num-list-format") =
        (s.m_num_prefix + "%" + std::to_string(s.m_level) + "%" + s.m_num_suffix).c_str();
    num_node.append_attribute("style:num-format") =
        std::string(1, static_cast<char>(s.m_format)).c_str();
    num_node.append_attribute("text:start-value") = s.m_start_from;
    num_node.append_attribute("style:num-suffix") = s.m_num_suffix.c_str();
    num_node.append_attribute("style:num-prefix") = s.m_num_prefix.c_str();
}

void write_xml(pugi::xml_node &n, const text_props &tp)
{
    auto text_props_node = n.append_child("style:text-properties");

    if(tp.m_font_size)
        text_props_node.append_attribute("fo:font-size") =
            (detail::to_string(tp.m_font_size->m_points) + "pt").c_str();

    if(tp.m_font_style)
        text_props_node.append_attribute("fo:font-style") =
            to_odf(font_style_names, tp.m_font_style.value());

    if(tp.m_font_name)
        text_props_node.append_attribute("style:font-name") = tp.m_font_name->get_name().c_str();

    if(tp.m_font_weight)
        text_props_node.append_attribute("fo:font-weight") = to_odf(tp.m_font_weight.value());

    if(tp.m_colour)
        text_props_node.append_attribute("fo:color") = detail::to_hex(*tp.m_colour).c_str();
}

void write_xml(pugi::xml_node &n, const paragraph_props &props)
{
    auto para_props_node = n.append_child("style:paragraph-properties");

    if(props.m_break_after)
        para_props_node.append_attribute("fo:break-after") =
            to_odf(break_names, props.m_break_after->m_break_type);
    if(props.m_break_before)
        para_props_node.append_attribute("fo:break-before") =
            to_odf(break_names, props.m_break_before->m_break_type);
}

void write_xml(pugi::xml_node &n, const graphics_props &props)
{
    auto graphic_node = n.append_child("style:graphic-properties");

    if(props.m_horiz_pos)
        graphic_node.append_attribute("style:horizontal-pos") =
            to_odf(horiz_names, *props.m_horiz_pos);
    if(props.m_vert_pos)
        graphic_node.append_attribute("style:vertical-pos") = to_odf(vert_names, *props.m_vert_pos);
}

std::optional<style> read_style(const pugi::xml_node &n)
{
    const auto family = from_odf(family_names, n.attribute("style:family").as_string());
    if(!family)
        return std::nullopt;

    style s(style_name(n.attribute("style:name").as_string()), *family);
    s.m_parent_style = style_name(n.attribute("style:parent-style-name").as_string());
    s.m_graphics_props = read_graphics_props(n.child("style:graphic-properties"));
    s.m_paragraph_props = read_paragraph_props(n.child("style:paragraph-properties"));
    s.m_text_props = read_text_props(n.child("style:text-properties"));
    return s;
}

list_style read_list_style(const pugi::xml_node &n)
{
    list_style ls(style_name(n.attribute("style:name").as_string()), {});
    for(auto level : n.children())
    {
        const std::string_view tag = level.name();
        const int lvl = level.attribute("text:level").as_int(1);
        const style_name sn(level.attribute("text:style-name").as_string());
        if(tag == "text:list-level-style-number")
        {
            const auto fmt = list_enum_from(level.attribute("style:num-format").as_string());
            if(!fmt)
                continue;
            list_style_num num(style{},
                lvl,
                *fmt,
                level.attribute("style:num-suffix").as_string(),
                level.attribute("text:start-value").as_int(1),
                level.attribute("style:num-prefix").as_string());
            num.m_style_name = sn;
            ls.m_level_styles.push_back(num);
        }
        else if(tag == "text:list-level-style-bullet")
        {
            list_style_bullet bullet(style{}, lvl, level.attribute("text:bullet-char").as_string());
            bullet.m_style_name = sn;
            ls.m_level_styles.push_back(bullet);
        }
    }
    return ls;
}

void read_styles(const odf::package &pkg, text_doc &doc)
{
    auto read_part = [&](const char *name, const char *root, auto &&read)
    {
        const auto *e = pkg.find(name);
        pugi::xml_document xml;
        if(e && xml.load_buffer(e->data.data(), e->data.size()))
            read(xml.child(root));
    };
    read_part("styles.xml",
        "office:document-styles",
        [&](const pugi::xml_node &r) { read_into(r.child("office:styles"), doc); });
    // Earlier versions of this library wrote common styles into content.xml.
    read_part("content.xml",
        "office:document-content",
        [&](const pugi::xml_node &r)
        {
            read_into(r.child("office:styles"), doc);
            read_into(r.child("office:automatic-styles"), doc);
        });
}

} // namespace docsmith::odt
