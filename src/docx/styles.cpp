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

#include <algorithm>
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

struct border_spelling
{
    border_line line;
    const char *docx;
};
constexpr border_spelling border_names[] = {{border_line::solid, "single"},
    {border_line::dashed, "dashed"},
    {border_line::dotted, "dotted"},
    {border_line::double_line, "double"},
    {border_line::none, "nil"}};

/// A border element (w:top, w:insideH, ...): its width is in eighths of a
/// point.
void write_border(pugi::xml_node parent, const char *name, const border &b)
{
    auto n = parent.append_child(name);
    const char *val = "single";
    for(const auto &s : border_names)
        if(s.line == b.m_line)
            val = s.docx;
    set_val(n, val);
    if(b.m_line == border_line::none)
        return;
    n.append_attribute("w:sz") = std::clamp(static_cast<int>(b.m_width.m_points * 8 + 0.5), 2, 96);
    n.append_attribute("w:space") = 0;
    n.append_attribute("w:color") = detail::to_hex(b.m_colour).substr(1).c_str();
}

std::optional<border> read_border(const pugi::xml_node &n)
{
    if(!n)
        return std::nullopt;
    const std::string v = n.attribute("w:val").as_string();
    if(v == "nil" || v == "none")
        return border::none();
    border b;
    b.m_line = border_line::solid;
    for(const auto &s : border_names)
        if(v == s.docx)
            b.m_line = s.line;
    b.m_width = pt(n.attribute("w:sz").as_double(4) / 8);
    if(auto c = colour_from(n.attribute("w:color").as_string()))
        b.m_colour = *c;
    return b;
}

/// A w:tcMar or w:tblCellMar, the same on every side.
void write_margins(pugi::xml_node n, const length &l)
{
    for(const char *side : {"w:top", "w:left", "w:bottom", "w:right"})
    {
        auto m = n.append_child(side);
        m.append_attribute("w:w") = twips(l);
        m.append_attribute("w:type") = "dxa";
    }
}

/// The margin, if every side has the same one.
std::optional<length> read_margins(const pugi::xml_node &n)
{
    std::optional<int> w;
    for(const char *side : {"w:top", "w:left", "w:bottom", "w:right"})
    {
        auto m = n.child(side);
        if(!m)
            return std::nullopt;
        const int v = m.attribute("w:w").as_int();
        if(w && *w != v)
            return std::nullopt;
        w = v;
    }
    if(!w)
        return std::nullopt;
    return pt(*w / 20.0);
}

/// The level's number or bullet text style and properties, as its w:rPr.
void write_level_rpr(pugi::xml_node lvl, const text_style_name &sn, const text_props &tp)
{
    if(sn.empty() && tp.empty())
        return;
    auto rpr = ordered_child(lvl, "w:rPr", lvl_order);
    if(!sn.empty())
        set_val(ordered_child(rpr, "w:rStyle", rpr_order), sn.str());
    if(!tp.empty())
        write_rpr(rpr, tp);
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
    case style_family::para: return "paragraph";
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
            f.append_attribute(a) = props.m_font_name->str().c_str();
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
    auto on_off = [&](const char *name, const std::optional<bool> &v)
    {
        if(!v)
            return;
        auto n = ordered_child(ppr, name, ppr_order);
        if(!*v)
            set_val(n, "0");
    };
    on_off("w:keepNext", props.m_keep_with_next);
    on_off("w:keepLines", props.m_keep_together);
    if(props.m_break_before && props.m_break_before->m_break_type == break_type::page)
        ordered_child(ppr, "w:pageBreakBefore", ppr_order);
    auto tw = [](const length &l) { return std::to_string(std::lround(l.m_points * 20)); };
    auto set = [](pugi::xml_node n, const char *attr, const std::string &v)
    {
        if(!n.attribute(attr))
            n.append_attribute(attr);
        n.attribute(attr).set_value(v.c_str());
    };
    if(props.m_space_before || props.m_space_after)
    {
        auto sp = ordered_child(ppr, "w:spacing", ppr_order);
        if(props.m_space_before)
            set(sp, "w:before", tw(*props.m_space_before));
        if(props.m_space_after)
            set(sp, "w:after", tw(*props.m_space_after));
    }
    if(props.m_indent_start || props.m_indent_end || props.m_first_line_indent)
    {
        auto ind = ordered_child(ppr, "w:ind", ppr_order);
        if(props.m_indent_start)
            set(ind, "w:left", tw(*props.m_indent_start));
        if(props.m_indent_end)
            set(ind, "w:right", tw(*props.m_indent_end));
        if(const auto &f = props.m_first_line_indent)
        {
            ind.remove_attribute("w:firstLine");
            ind.remove_attribute("w:hanging");
            if(f->m_points < 0)
                set(ind, "w:hanging", tw(pt(-f->m_points)));
            else
                set(ind, "w:firstLine", tw(*f));
        }
    }
    if(props.m_align)
    {
        // start and end as left and right: older Word reads only those.
        const char *jc = "left";
        switch(*props.m_align)
        {
        case text_align::start:
        case text_align::left: jc = "left"; break;
        case text_align::end:
        case text_align::right: jc = "right"; break;
        case text_align::centre: jc = "center"; break;
        case text_align::justify: jc = "both"; break;
        }
        set_val(ordered_child(ppr, "w:jc", ppr_order), jc);
    }
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
    if(auto k = ppr.child("w:keepNext"))
        pp.m_keep_with_next = on(k);
    if(auto k = ppr.child("w:keepLines"))
        pp.m_keep_together = on(k);
    auto tw = [](const pugi::xml_attribute &a) -> std::optional<length>
    {
        if(!a)
            return std::nullopt;
        return pt(a.as_double() / 20);
    };
    if(auto sp = ppr.child("w:spacing"))
    {
        pp.m_space_before = tw(sp.attribute("w:before"));
        pp.m_space_after = tw(sp.attribute("w:after"));
    }
    if(auto ind = ppr.child("w:ind"))
    {
        auto a = ind.attribute("w:left");
        pp.m_indent_start = tw(a ? a : ind.attribute("w:start"));
        a = ind.attribute("w:right");
        pp.m_indent_end = tw(a ? a : ind.attribute("w:end"));
        if(auto h = tw(ind.attribute("w:hanging")))
            pp.m_first_line_indent = pt(-h->m_points);
        else
            pp.m_first_line_indent = tw(ind.attribute("w:firstLine"));
    }
    const std::string jc = val(ppr, "w:jc");
    if(jc == "left")
        pp.set(text_align::left);
    else if(jc == "start")
        pp.set(text_align::start);
    else if(jc == "right")
        pp.set(text_align::right);
    else if(jc == "end")
        pp.set(text_align::end);
    else if(jc == "center")
        pp.set(text_align::centre);
    else if(jc == "both" || jc == "distribute")
        pp.set(text_align::justify);
    return pp == paragraph_props{} ? std::nullopt : std::optional(std::move(pp));
}

void write_tcpr(pugi::xml_node tc_pr, const cell_props &props)
{
    const auto &b = props.m_borders;
    if(b != cell_borders{})
    {
        auto borders = tc_pr.append_child("w:tcBorders");
        for(auto [name, side] : {std::pair{"w:top", &b.m_top},
                std::pair{"w:left", &b.m_start},
                std::pair{"w:bottom", &b.m_bottom},
                std::pair{"w:right", &b.m_end}})
            if(*side)
                write_border(borders, name, **side);
    }
    if(props.m_background)
    {
        auto shd = tc_pr.append_child("w:shd");
        set_val(shd, "clear");
        shd.append_attribute("w:color") = "auto";
        shd.append_attribute("w:fill") = detail::to_hex(*props.m_background).substr(1).c_str();
    }
    if(props.m_padding)
        write_margins(tc_pr.append_child("w:tcMar"), *props.m_padding);
    if(props.m_valign)
        set_val(tc_pr.append_child("w:vAlign"),
            *props.m_valign == cell_valign::middle       ? "center"
                : *props.m_valign == cell_valign::bottom ? "bottom"
                                                         : "top");
}

cell_props read_tcpr(const pugi::xml_node &tc_pr)
{
    cell_props p;
    if(auto borders = tc_pr.child("w:tcBorders"))
    {
        auto side = [&](const char *name, const char *alt)
        { return read_border(borders.child(name) ? borders.child(name) : borders.child(alt)); };
        p.m_borders.m_top = side("w:top", "w:top");
        p.m_borders.m_start = side("w:left", "w:start");
        p.m_borders.m_bottom = side("w:bottom", "w:bottom");
        p.m_borders.m_end = side("w:right", "w:end");
    }
    if(auto c = colour_from(tc_pr.child("w:shd").attribute("w:fill").as_string()))
        p.set(*c);
    if(auto m = read_margins(tc_pr.child("w:tcMar")))
        p.set(cell_padding{*m});
    if(const std::string v = val(tc_pr, "w:vAlign"); !v.empty())
        p.set(v == "center"     ? cell_valign::middle
                : v == "bottom" ? cell_valign::bottom
                                : cell_valign::top);
    return p;
}

void write_style_tblpr(pugi::xml_node tbl_pr, const table_props &props)
{
    const auto &o = props.m_outer;
    if(o != cell_borders{} || props.m_inside_h || props.m_inside_v)
    {
        auto borders = tbl_pr.append_child("w:tblBorders");
        for(auto [name, side] : {std::pair{"w:top", &o.m_top},
                std::pair{"w:left", &o.m_start},
                std::pair{"w:bottom", &o.m_bottom},
                std::pair{"w:right", &o.m_end},
                std::pair{"w:insideH", &props.m_inside_h},
                std::pair{"w:insideV", &props.m_inside_v}})
            if(*side)
                write_border(borders, name, **side);
    }
    if(props.m_padding)
        write_margins(tbl_pr.append_child("w:tblCellMar"), *props.m_padding);
}

std::optional<table_props> read_style_tblpr(const pugi::xml_node &tbl_pr)
{
    table_props p;
    if(auto borders = tbl_pr.child("w:tblBorders"))
    {
        auto side = [&](const char *name, const char *alt)
        { return read_border(borders.child(name) ? borders.child(name) : borders.child(alt)); };
        p.m_outer.m_top = side("w:top", "w:top");
        p.m_outer.m_start = side("w:left", "w:start");
        p.m_outer.m_bottom = side("w:bottom", "w:bottom");
        p.m_outer.m_end = side("w:right", "w:end");
        p.m_inside_h = side("w:insideH", "w:insideH");
        p.m_inside_v = side("w:insideV", "w:insideV");
    }
    if(auto m = read_margins(tbl_pr.child("w:tblCellMar")))
        p.set(cell_padding{*m});
    return p == table_props{} ? std::nullopt : std::optional(std::move(p));
}

namespace
{
/// The w:style's own elements, before its properties.
void style_head(pugi::xml_node n, const char *type, const std::string &id,
    const std::string &based_on, const std::string &link)
{
    n.append_attribute("w:type") = type;
    n.append_attribute("w:styleId") = id.c_str();
    set_val(n.append_child("w:name"), word_style_name(id));
    if(!based_on.empty())
        set_val(n.append_child("w:basedOn"), based_on);
    if(!link.empty())
        set_val(n.append_child("w:link"), link);
    n.append_child("w:qFormat");
}

/// Removes the empty property elements.
void drop_empty(pugi::xml_node n)
{
    for(auto c = n.first_child(); c;)
    {
        auto next = c.next_sibling();
        const std::string_view name = c.name();
        if((name == "w:pPr" || name == "w:rPr" || name == "w:tblPr") && !c.first_child())
            n.remove_child(c);
        c = next;
    }
}
} // namespace

void write_style(pugi::xml_node n, const paragraph_style &s, const std::string &id,
    const std::string &based_on, const std::string &link)
{
    style_head(n, "paragraph", id, based_on, link);
    if(!s.para.empty())
        write_ppr(n.append_child("w:pPr"), s.para);
    if(!s.text.empty())
        write_rpr(n.append_child("w:rPr"), s.text);
    drop_empty(n);
}

void write_style(pugi::xml_node n, const text_style &s, const std::string &id,
    const std::string &based_on, const std::string &link)
{
    style_head(n, "character", id, based_on, link);
    if(!s.text.empty())
        write_rpr(n.append_child("w:rPr"), s.text);
    drop_empty(n);
}

void write_style(pugi::xml_node n, const table_style &s, const std::string &id,
    const std::string &based_on, const std::string &link)
{
    style_head(n, "table", id, based_on, link);
    if(!s.table.empty())
        write_style_tblpr(n.append_child("w:tblPr"), s.table);
    drop_empty(n);
}

std::optional<docx_style> read_style(
    const pugi::xml_node &n, const std::string &name, const std::string &parent)
{
    const std::string_view type = n.attribute("w:type").as_string("paragraph");
    if(type == "paragraph")
    {
        paragraph_style s(name,
            read_ppr(n.child("w:pPr")).value_or(paragraph_props{}),
            read_rpr(n.child("w:rPr")).value_or(text_props{}));
        s.parent = parent;
        return s;
    }
    if(type == "character")
    {
        text_style s(name, read_rpr(n.child("w:rPr")).value_or(text_props{}));
        s.parent = parent;
        return s;
    }
    if(type == "table")
    {
        table_style s(name, read_style_tblpr(n.child("w:tblPr")).value_or(table_props{}));
        s.parent = parent;
        return s;
    }
    return std::nullopt;
}

void write_abstract_num(pugi::xml_node n, int id, const list_style &ls)
{
    n.append_attribute("w:abstractNumId") = id;
    set_val(n.append_child("w:multiLevelType"), "hybridMultilevel");
    set_val(n.append_child("w:name"), ls.name().str());
    for(const auto &level : ls.levels)
        std::visit(
            overloaded{
                [&](const number_level &num)
                {
                    auto lvl = add_level(n, num.level);
                    set_val(lvl.append_child("w:start"), std::to_string(num.start_from));
                    const char *fmt = "decimal";
                    for(const auto &[e, name] : num_formats)
                        if(e == num.format)
                            fmt = name;
                    set_val(lvl.append_child("w:numFmt"), fmt);
                    set_val(lvl.append_child("w:lvlText"),
                        num.prefix + "%" + std::to_string(std::max(num.level, 1)) + num.suffix);
                    write_indent(lvl, num.level);
                    write_level_rpr(lvl, num.style, num.text);
                },
                [&](const bullet_level &b)
                {
                    auto lvl = add_level(n, b.level);
                    set_val(lvl.append_child("w:start"), "1");
                    set_val(lvl.append_child("w:numFmt"), "bullet");
                    set_val(lvl.append_child("w:lvlText"), b.bullet);
                    write_indent(lvl, b.level);
                    write_level_rpr(lvl, b.style, {});
                },
            },
            level);
}

list_style read_abstract_num(const pugi::xml_node &n, const list_style_name &name)
{
    list_style ls(name);
    for(auto lvl : n.children("w:lvl"))
    {
        if(lvl.child("w:lvlPicBulletId"))
            continue;
        const int level = lvl.attribute("w:ilvl").as_int() + 1;
        const std::string fmt = val(lvl, "w:numFmt");
        const std::string text = val(lvl, "w:lvlText");
        const auto rpr = lvl.child("w:rPr");
        const text_style_name rstyle(val(rpr, "w:rStyle"));
        if(fmt == "bullet")
        {
            ls.levels.emplace_back(bullet_level{.level = level, .bullet = text, .style = rstyle});
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
        ls.levels.emplace_back(number_level{.level = level,
            .format = *format,
            .prefix = text.substr(0, first),
            .suffix = text.substr(end),
            .start_from = lvl.child("w:start").attribute("w:val").as_int(1),
            .style = rstyle,
            .text = read_rpr(rpr).value_or(text_props{})});
    }
    return ls;
}

} // namespace docsmith::docx
