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
#include "docx/styles.h"

#include <charconv>
#include <cmath>
#include <string_view>

#include "docx/ooxml.h"
#include "format.h"

namespace docsmith::docx
{
namespace
{
constexpr std::pair<list_enum, const char *> num_formats[] = {{list_enum::arabic, "decimal"},
    {list_enum::lower_alpha, "lowerLetter"},
    {list_enum::upper_alpha, "upperLetter"},
    {list_enum::lower_roman, "lowerRoman"},
    {list_enum::upper_roman, "upperRoman"}};

constexpr std::array<const char *, 12> lvl_order{"w:start",
    "w:numFmt",
    "w:lvlRestart",
    "w:pStyle",
    "w:isLgl",
    "w:suff",
    "w:lvlText",
    "w:lvlPicBulletId",
    "w:legacy",
    "w:lvlJc",
    "w:pPr",
    "w:rPr"};

void set_val(pugi::xml_node n, const std::string &v) { n.append_attribute("w:val") = v.c_str(); }

std::optional<colour> colour_from(std::string_view hex)
{
    if(hex.size() != 6)
        return std::nullopt;
    std::uint8_t c[3]{};
    for(int i = 0; i != 3; ++i)
        if(auto [p, ec] = std::from_chars(hex.data() + 2 * i, hex.data() + 2 * i + 2, c[i], 16);
            ec != std::errc{} || p != hex.data() + 2 * i + 2)
            return std::nullopt;
    return colour::rgb(c[0], c[1], c[2]);
}

/// The level's number or bullet text style and properties, as its w:rPr.
void write_level_rpr(pugi::xml_node lvl, const style_name &sn, const std::optional<text_props> &tp)
{
    if(sn.is_empty() && !tp)
        return;
    auto rpr = ordered_child(lvl, "w:rPr", lvl_order);
    if(!sn.is_empty())
        set_val(ordered_child(rpr, "w:rStyle", rpr_order), sn.get_name());
    if(tp)
        write_rpr(rpr, *tp);
}

pugi::xml_node add_level(pugi::xml_node n, int level)
{
    auto lvl = n.append_child("w:lvl");
    lvl.append_attribute("w:ilvl") = std::max(level - 1, 0);
    return lvl;
}

void write_indent(pugi::xml_node lvl, int level)
{
    set_val(ordered_child(lvl, "w:lvlJc", lvl_order), "left");
    auto ind = ordered_child(ordered_child(lvl, "w:pPr", lvl_order), "w:ind", ppr_order);
    ind.append_attribute("w:left") = list_indent(std::max(level - 1, 0));
    ind.append_attribute("w:hanging") = list_hanging;
}
} // namespace

const char *style_type(style_family f)
{
    switch(f)
    {
    case style_family::paragraph: return "paragraph";
    case style_family::text: return "character";
    case style_family::table: return "table";
    default: return nullptr;
    }
}

void write_rpr(pugi::xml_node rpr, const text_props &props)
{
    if(props.m_font_name)
    {
        auto f = ordered_child(rpr, "w:rFonts", rpr_order);
        for(const char *a : {"w:ascii", "w:hAnsi", "w:cs"})
            f.append_attribute(a) = props.m_font_name->get_name().c_str();
    }
    if(props.m_font_weight)
    {
        auto b = ordered_child(rpr, "w:b", rpr_order);
        if(*props.m_font_weight != font_weight::bold)
            set_val(b, "0");
    }
    if(props.m_font_style)
    {
        auto i = ordered_child(rpr, "w:i", rpr_order);
        if(*props.m_font_style == font_style::normal)
            set_val(i, "0");
    }
    if(props.m_colour)
    {
        std::string hex = detail::to_hex(*props.m_colour).substr(1);
        for(auto &c : hex)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        set_val(ordered_child(rpr, "w:color", rpr_order), hex);
    }
    if(props.m_font_size)
    {
        const auto half_points = std::to_string(std::lround(props.m_font_size->m_points * 2));
        set_val(ordered_child(rpr, "w:sz", rpr_order), half_points);
        set_val(ordered_child(rpr, "w:szCs", rpr_order), half_points);
    }
}

void write_ppr(pugi::xml_node ppr, const paragraph_props &props)
{
    if(props.m_break_before && props.m_break_before->m_break_type == break_type::page)
        ordered_child(ppr, "w:pageBreakBefore", ppr_order);
}

std::optional<text_props> read_rpr(const pugi::xml_node &rpr)
{
    if(!rpr)
        return std::nullopt;
    text_props tp;
    if(auto sz = rpr.child("w:sz"))
        tp.set(font_size(sz.attribute("w:val").as_float() / 2));
    if(auto i = rpr.child("w:i"))
        tp.set(on(i) ? font_style::italic : font_style::normal);
    if(auto f = rpr.child("w:rFonts"))
    {
        const char *name = f.attribute("w:ascii").as_string(f.attribute("w:hAnsi").as_string());
        if(*name)
            tp.set(font_name(name));
    }
    if(auto b = rpr.child("w:b"))
        tp.set(on(b) ? font_weight::bold : font_weight::normal);
    if(auto c = colour_from(val(rpr, "w:color")))
        tp.set(*c);
    return tp == text_props{} ? std::nullopt : std::optional(std::move(tp));
}

std::optional<paragraph_props> read_ppr(const pugi::xml_node &ppr)
{
    if(!ppr)
        return std::nullopt;
    paragraph_props pp;
    if(auto b = ppr.child("w:pageBreakBefore"); b && on(b))
        pp.set(break_before{break_type::page});
    return pp == paragraph_props{} ? std::nullopt : std::optional(std::move(pp));
}

void write_style(pugi::xml_node n, const style &s, const char *type, const std::string &id,
    const std::string &based_on, const std::string &link)
{
    n.append_attribute("w:type") = type;
    n.append_attribute("w:styleId") = id.c_str();
    set_val(n.append_child("w:name"), display_name(id));
    if(!based_on.empty())
        set_val(n.append_child("w:basedOn"), based_on);
    if(!link.empty())
        set_val(n.append_child("w:link"), link);
    n.append_child("w:qFormat");
    const std::string_view t = type;
    if(t == "paragraph" && s.m_paragraph_props)
        write_ppr(n.append_child("w:pPr"), *s.m_paragraph_props);
    if((t == "paragraph" || t == "character") && s.m_text_props)
        write_rpr(n.append_child("w:rPr"), *s.m_text_props);
    for(auto c = n.first_child(); c;)
    {
        auto next = c.next_sibling();
        if((std::string_view(c.name()) == "w:pPr" || std::string_view(c.name()) == "w:rPr") &&
            !c.first_child())
            n.remove_child(c);
        c = next;
    }
}

std::optional<style> read_style(const pugi::xml_node &n)
{
    const std::string_view type = n.attribute("w:type").as_string("paragraph");
    style s(style_name(n.attribute("w:styleId").as_string()));
    if(type == "paragraph")
        s.set(style_family::paragraph);
    else if(type == "character")
        s.set(style_family::text);
    else if(type == "table")
        s.set(style_family::table);
    else
        return std::nullopt;
    s.m_parent_style = style_name(val(n, "w:basedOn"));
    if(type != "table")
        if(auto tp = read_rpr(n.child("w:rPr")))
            s.set(*tp);
    if(type == "paragraph")
        if(auto pp = read_ppr(n.child("w:pPr")))
            s.set(*pp);
    return s;
}

void write_abstract_num(pugi::xml_node n, int id, const list_style &ls)
{
    n.append_attribute("w:abstractNumId") = id;
    set_val(n.append_child("w:multiLevelType"), "hybridMultilevel");
    set_val(n.append_child("w:name"), ls.m_name.get_name());
    for(const auto &lvl_style : ls.m_level_styles)
        std::visit(
            overloaded{
                [&](const list_style_num &num)
                {
                    auto lvl = add_level(n, num.m_level);
                    set_val(lvl.append_child("w:start"), std::to_string(num.m_start_from));
                    const char *fmt = "decimal";
                    for(const auto &[e, name] : num_formats)
                        if(e == num.m_format)
                            fmt = name;
                    set_val(lvl.append_child("w:numFmt"), fmt);
                    set_val(lvl.append_child("w:lvlText"),
                        num.m_num_prefix + "%" + std::to_string(std::max(num.m_level, 1)) +
                            num.m_num_suffix);
                    write_indent(lvl, num.m_level);
                    write_level_rpr(lvl, num.m_style_name, num.m_text_props);
                },
                [&](const list_style_bullet &b)
                {
                    auto lvl = add_level(n, b.m_level);
                    set_val(lvl.append_child("w:start"), "1");
                    set_val(lvl.append_child("w:numFmt"), "bullet");
                    set_val(lvl.append_child("w:lvlText"), b.m_bullet_char);
                    write_indent(lvl, b.m_level);
                    write_level_rpr(lvl, b.m_style_name, std::nullopt);
                },
            },
            lvl_style);
}

list_style read_abstract_num(const pugi::xml_node &n, const style_name &name)
{
    list_style ls(name, {});
    for(auto lvl : n.children("w:lvl"))
    {
        if(lvl.child("w:lvlPicBulletId"))
            continue;
        const int level = lvl.attribute("w:ilvl").as_int() + 1;
        const std::string fmt = val(lvl, "w:numFmt");
        const std::string text = val(lvl, "w:lvlText");
        const auto rpr = lvl.child("w:rPr");
        const style rstyle(style_name(val(rpr, "w:rStyle")));
        if(fmt == "bullet")
        {
            ls.m_level_styles.emplace_back(list_style_bullet(rstyle, level, text));
            continue;
        }
        std::optional<list_enum> format;
        for(const auto &[e, f] : num_formats)
            if(fmt == f)
                format = e;
        // The number is the last placeholder: "%1.%2." shows the parent's too,
        // which the model doesn't hold.
        const auto first = text.find('%');
        const auto last = text.rfind('%');
        if(!format || last == std::string::npos)
            continue;
        auto end = last + 1;
        while(end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
            ++end;
        list_style_num num(rstyle,
            level,
            *format,
            text.substr(end),
            lvl.child("w:start").attribute("w:val").as_int(1),
            text.substr(0, first));
        num.m_text_props = read_rpr(rpr);
        ls.m_level_styles.emplace_back(std::move(num));
    }
    return ls;
}

} // namespace docsmith::docx
