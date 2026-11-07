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

#include <cctype>
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

#include "builtin_styles.h"
#include "format.h"
#include "odf/names.h"
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

constexpr spelling<font_style> font_style_names[] = {{font_style::normal, "normal"},
    {font_style::italic, "italic"},
    {font_style::oblique, "oblique"}};
constexpr spelling<break_type> break_names[] = {{break_type::automatic, "auto"},
    {break_type::column, "column"},
    {break_type::page, "page"},
    {break_type::even_page, "even-page"},
    {break_type::odd_page, "odd-page"}};
constexpr spelling<text_align> align_names[] = {{text_align::start, "start"},
    {text_align::end, "end"},
    {text_align::left, "left"},
    {text_align::right, "right"},
    {text_align::centre, "center"},
    {text_align::justify, "justify"}};
constexpr spelling<style_family> family_names[] = {{style_family::para, "paragraph"},
    {style_family::text, "text"},
    {style_family::frame, "graphic"},
    {style_family::table, "table"},
    {style_family::row, "table-row"},
    {style_family::cell, "table-cell"}};

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

} // namespace

std::optional<length> length_from(std::string_view odf)
{
    struct unit
    {
        std::string_view name;
        double points;
    };
    constexpr unit units[] = {
        {"pt", 1}, {"cm", 72 / 2.54}, {"mm", 72 / 25.4}, {"in", 72}, {"pc", 12}, {"px", 0.75}};
    for(const auto &u : units)
        if(odf.ends_with(u.name))
        {
            const std::string_view num = odf.substr(0, odf.size() - u.name.size());
            double v{};
            if(auto [p, ec] = std::from_chars(num.data(), num.data() + num.size(), v);
                ec != std::errc{} || p != num.data() + num.size())
                return std::nullopt;
            // To the thousandth of a point the writer keeps, so a length
            // reads back the same after a save.
            return length{std::round(v * u.points * 1000) / 1000};
        }
    return std::nullopt;
}

namespace
{
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

/// The first family in an svg:font-family or fo:font-family list, unquoted.
std::string first_family(std::string_view list)
{
    list = list.substr(0, list.find(','));
    auto trim = [](char c) { return c == ' ' || c == '\'' || c == '"'; };
    while(!list.empty() && trim(list.front()))
        list.remove_prefix(1);
    while(!list.empty() && trim(list.back()))
        list.remove_suffix(1);
    return std::string(list);
}

/// The font family a style:font-name declaration (in n's own document) names:
/// LibreOffice declares "Liberation Sans1" for a second "Liberation Sans".
/// The name itself if there is no declaration.
std::string font_family(const pugi::xml_node &n, const std::string &name)
{
    const auto decls = n.root().first_child().child("office:font-face-decls");
    const auto face = decls.find_child_by_attribute("style:font-face", "style:name", name.c_str());
    if(auto family = first_family(face.attribute("svg:font-family").as_string()); !family.empty())
        return family;
    return name;
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
    // font_name is the family, as in DOCX and CSS.
    if(auto a = n.attribute("style:font-name"))
        tp.set(font_name(font_family(n, a.as_string())));
    else if(auto f = first_family(n.attribute("fo:font-family").as_string()); !f.empty())
        tp.set(font_name(f));
    if(auto fw = font_weight_from(n.attribute("fo:font-weight").as_string()))
        tp.set(*fw);
    if(auto c = colour_from(n.attribute("fo:color").as_string()))
        tp.set(font_colour(*c));
    if(auto u = n.attribute("style:text-underline-style"))
        tp.set(std::string_view(u.as_string()) == "none" ? underline::none : underline::single);
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
    if(auto a = from_odf(align_names, n.attribute("fo:text-align").as_string()))
        pp.set(*a);
    // Lengths only: a percentage of the parent's is dropped.
    auto len = [&](const char *attr) { return length_from(n.attribute(attr).as_string()); };
    pp.m_space_before = len("fo:margin-top");
    pp.m_space_after = len("fo:margin-bottom");
    pp.m_indent_start = len("fo:margin-left");
    pp.m_indent_end = len("fo:margin-right");
    pp.m_first_line_indent = len("fo:text-indent");
    if(auto k = n.attribute("fo:keep-with-next"))
        pp.m_keep_with_next = std::string_view(k.as_string()) == "always";
    if(auto k = n.attribute("fo:keep-together"))
        pp.m_keep_together = std::string_view(k.as_string()) == "always";
    return pp == paragraph_props{} ? std::nullopt : std::optional(std::move(pp));
}

constexpr spelling<border_line> border_names[] = {{border_line::none, "none"},
    {border_line::solid, "solid"},
    {border_line::dashed, "dashed"},
    {border_line::dotted, "dotted"},
    {border_line::double_line, "double"}};
constexpr spelling<cell_valign> valign_names[] = {
    {cell_valign::top, "top"}, {cell_valign::middle, "middle"}, {cell_valign::bottom, "bottom"}};
constexpr spelling<table_align> table_align_names[] = {
    {table_align::start, "left"}, {table_align::centre, "center"}, {table_align::end, "right"}};

/// "0.5pt solid #000000", in any order; "none" and "hidden" are no line.
std::optional<border> border_from(std::string_view odf)
{
    if(odf.empty())
        return std::nullopt;
    border b;
    while(!odf.empty())
    {
        const auto end = odf.find(' ');
        const std::string_view word = odf.substr(0, end);
        odf = end == std::string_view::npos ? std::string_view{} : odf.substr(end + 1);
        if(word == "hidden" || word == "none")
            return border::none();
        if(auto l = from_odf(border_names, word))
            b.m_line = *l;
        else if(auto c = colour_from(word))
            b.m_colour = *c;
        else if(auto w = length_from(word))
            b.m_width = *w;
    }
    return b;
}

std::string to_odf(const border &b)
{
    if(b.m_line == border_line::none)
        return "none";
    return detail::to_string(b.m_width.m_points) + "pt " + to_odf(border_names, b.m_line) + " " +
        detail::to_hex(b.m_colour);
}

std::string to_odf(const length &l) { return detail::to_string(l.m_points) + "pt"; }

std::optional<table_props> read_table_props(const pugi::xml_node &n)
{
    if(!n)
        return std::nullopt;
    table_props tp;
    const std::string_view align = n.attribute("table:align").as_string();
    // A table aligned to the margins fills them, whatever its width says.
    if(align != "margins")
    {
        if(auto a = from_odf(table_align_names, align))
            tp.set(*a);
        const std::string_view rel = n.attribute("style:rel-width").as_string();
        double pc{};
        if(rel.ends_with('%') &&
            std::from_chars(rel.data(), rel.data() + rel.size() - 1, pc).ec == std::errc{})
            tp.set(table_width(percent(pc)));
        else if(auto w = length_from(n.attribute("style:width").as_string()))
            tp.set(table_width(*w));
    }
    return tp == table_props{} ? std::nullopt : std::optional(std::move(tp));
}

std::optional<cell_props> read_cell_props(const pugi::xml_node &n)
{
    if(!n)
        return std::nullopt;
    cell_props cp;
    auto &b = cp.m_borders;
    if(auto all = border_from(n.attribute("fo:border").as_string()))
        b.m_top = b.m_bottom = b.m_start = b.m_end = all;
    for(auto [attr, side] : {std::pair{"fo:border-top", &b.m_top},
            std::pair{"fo:border-bottom", &b.m_bottom},
            std::pair{"fo:border-left", &b.m_start},
            std::pair{"fo:border-right", &b.m_end}})
        if(auto one = border_from(n.attribute(attr).as_string()))
            *side = one;
    if(auto c = colour_from(n.attribute("fo:background-color").as_string()))
        cp.set(background(*c));
    if(auto v = from_odf(valign_names, n.attribute("style:vertical-align").as_string()))
        cp.set(*v);
    if(auto p = length_from(n.attribute("fo:padding").as_string()))
        cp.set(cell_padding{*p});
    return cp.empty() ? std::nullopt : std::optional(std::move(cp));
}

void read_into(const pugi::xml_node &container, text_doc &doc)
{
    for(auto c : container.children())
    {
        const std::string_view tag = c.name();
        if(tag == "style:style")
        {
            if(page_break_base(c))
                continue; // made by the writer for a page_break node
            // A column style's only modelled property, its width, is read
            // onto the table's columns instead (see odt::file::load()).
            if(std::string_view(c.attribute("style:family").as_string()) == "table-column")
                continue;
            if(auto s = read_style(c))
                std::visit([&](const auto &def) { doc.styles().insert(def); }, *s);
        }
        else if(tag == "text:list-style")
        {
            doc.styles().insert(read_list_style(c));
        }
    }
}
} // namespace

void write_name(pugi::xml_node &n, const std::string &name)
{
    const std::string odf = odf::odf_name(name);
    n.append_attribute("style:name").set_value(odf.c_str());
    // LibreOffice finds its own styles by style:name, so a style named
    // Heading_20_1 with another display name would be a new style.
    if(const std::string shown = odf::decode_name(odf); shown != odf)
        n.append_attribute("style:display-name").set_value(shown.c_str());
}

const char *family_name(style_family f) { return to_odf(family_names, f); }

namespace
{
/// A style:style for s, with its name, family and parent.
template <typename S>
pugi::xml_node style_node(pugi::xml_node &n, const S &s)
{
    auto node = n.append_child("style:style");
    write_name(node, s.name().str());
    node.append_attribute("style:family") = family_name(S::family);
    if constexpr(requires { s.parent; })
        if(!s.parent.empty())
            node.append_attribute("style:parent-style-name") =
                odf::odf_name(s.parent.str()).c_str();
    return node;
}
} // namespace

void write_xml(pugi::xml_node &n, const paragraph_style &s)
{
    auto node = style_node(n, s);
    if(const int level = detail::builtin_heading_level(s.name()))
        node.append_attribute("style:default-outline-level") = level;
    if(!s.para.empty())
        write_xml(node, s.para);
    if(!s.text.empty())
        write_xml(node, s.text);
}

void write_xml(pugi::xml_node &n, const text_style &s)
{
    auto node = style_node(n, s);
    if(!s.text.empty())
        write_xml(node, s.text);
}

void write_xml(pugi::xml_node &n, const table_style &s)
{
    auto node = style_node(n, s);
    // A table style's lines and header go on its cells (see the writer).
    write_xml(node, s.table);
}

void write_xml(pugi::xml_node &n, const cell_style &s)
{
    auto node = style_node(n, s);
    if(!s.cell.empty())
        write_xml(node, s.cell);
    // Kept for other readers: LibreOffice ignores them in a cell style, so the
    // writer also puts them on the cell's paragraphs (detail::with_cell_text).
    if(!s.para.empty())
        write_xml(node, s.para);
    if(!s.text.empty())
        write_xml(node, s.text);
}

void write_xml(pugi::xml_node &n, const row_style &s) { style_node(n, s); }

void write_xml(pugi::xml_node &n, const frame_style &s) { style_node(n, s); }

void write_xml(pugi::xml_node &n, const list_style &ls)
{
    auto ls_node = n.append_child("text:list-style");
    write_name(ls_node, ls.name().str());

    for(const auto &level : ls.levels)
        std::visit([&](const auto &l) { write_xml(ls_node, l); }, level);
}

void write_xml(pugi::xml_node &n, const bullet_level &b)
{
    auto bullet_node = n.append_child("text:list-level-style-bullet");
    bullet_node.append_attribute("text:level") = b.level;
    if(!b.style.empty())
        bullet_node.append_attribute("text:style-name") = odf::odf_name(b.style.str()).c_str();
    bullet_node.append_attribute("text:bullet-char") = b.bullet.c_str();
}

void write_xml(pugi::xml_node &n, const number_level &s)
{
    auto num_node = n.append_child("text:list-level-style-number");
    num_node.append_attribute("text:level") = s.level;
    if(!s.style.empty())
        num_node.append_attribute("text:style-name") = odf::odf_name(s.style.str()).c_str();

    num_node.append_attribute("loext:num-list-format") =
        (s.prefix + "%" + std::to_string(s.level) + "%" + s.suffix).c_str();
    num_node.append_attribute("style:num-format") =
        std::string(1, static_cast<char>(s.format)).c_str();
    num_node.append_attribute("text:start-value") = s.start_from;
    num_node.append_attribute("style:num-suffix") = s.suffix.c_str();
    num_node.append_attribute("style:num-prefix") = s.prefix.c_str();
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
        text_props_node.append_attribute("style:font-name") = tp.m_font_name->str().c_str();

    if(tp.m_font_weight)
        text_props_node.append_attribute("fo:font-weight") = to_odf(tp.m_font_weight.value());

    if(tp.m_font_colour)
        text_props_node.append_attribute("fo:color") = detail::to_hex(*tp.m_font_colour).c_str();

    if(tp.m_underline == underline::none)
        text_props_node.append_attribute("style:text-underline-style") = "none";
    else if(tp.m_underline == underline::single)
    {
        text_props_node.append_attribute("style:text-underline-style") = "solid";
        text_props_node.append_attribute("style:text-underline-width") = "auto";
        text_props_node.append_attribute("style:text-underline-color") = "font-color";
    }
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
    auto len = [&](const char *attr, const std::optional<length> &l)
    {
        if(l)
            para_props_node.append_attribute(attr) = to_odf(*l).c_str();
    };
    len("fo:margin-top", props.m_space_before);
    len("fo:margin-bottom", props.m_space_after);
    len("fo:margin-left", props.m_indent_start);
    len("fo:margin-right", props.m_indent_end);
    len("fo:text-indent", props.m_first_line_indent);
    if(props.m_align)
        para_props_node.append_attribute("fo:text-align") = to_odf(align_names, *props.m_align);
    if(props.m_keep_with_next)
        para_props_node.append_attribute("fo:keep-with-next") =
            *props.m_keep_with_next ? "always" : "auto";
    if(props.m_keep_together)
        para_props_node.append_attribute("fo:keep-together") =
            *props.m_keep_together ? "always" : "auto";
}

void write_xml(pugi::xml_node &n, const table_props &props)
{
    auto node = n.append_child("style:table-properties");
    // Text width of the writer's default page (A4, 2cm margins), for a width
    // given as a percentage.
    constexpr double text_width = 17 * 72 / 2.54;
    if(props.m_width)
    {
        if(const auto *pc = std::get_if<percent>(&*props.m_width))
        {
            node.append_attribute("style:width") =
                to_odf(length{text_width * pc->m_value / 100}).c_str();
            node.append_attribute("style:rel-width") =
                (detail::to_string(pc->m_value) + "%").c_str();
        }
        else
            node.append_attribute("style:width") = to_odf(std::get<length>(*props.m_width)).c_str();
    }
    node.append_attribute("table:align") = props.m_align ? to_odf(table_align_names, *props.m_align)
                                                         : (props.m_width ? "left" : "margins");
}

void write_xml(pugi::xml_node &n, const cell_props &props)
{
    auto node = n.append_child("style:table-cell-properties");
    for(auto [attr, side] : {std::pair{"fo:border-top", &props.m_borders.m_top},
            std::pair{"fo:border-bottom", &props.m_borders.m_bottom},
            std::pair{"fo:border-left", &props.m_borders.m_start},
            std::pair{"fo:border-right", &props.m_borders.m_end}})
        if(*side)
            node.append_attribute(attr) = to_odf(**side).c_str();
    if(props.m_background)
        node.append_attribute("fo:background-color") = detail::to_hex(*props.m_background).c_str();
    if(props.m_valign)
        node.append_attribute("style:vertical-align") = to_odf(valign_names, *props.m_valign);
    if(props.m_padding)
        node.append_attribute("fo:padding") = to_odf(*props.m_padding).c_str();
}

std::optional<double> read_column_width(const pugi::xml_node &n)
{
    auto props = n.child("style:table-column-properties");
    const std::string_view rel = props.attribute("style:rel-column-width").as_string();
    double v{};
    if(rel.ends_with('*') &&
        std::from_chars(rel.data(), rel.data() + rel.size() - 1, v).ec == std::errc{} && v > 0)
        return v;
    if(auto w = length_from(props.attribute("style:column-width").as_string()))
        return w->m_points;
    return std::nullopt;
}

constexpr std::string_view page_break_suffix = ".PageBreak";

std::string page_break_style_name(const para_style_name &base)
{
    return base.empty() ? std::string(page_break_suffix.substr(1))
                        : base.str() + std::string(page_break_suffix);
}

std::optional<para_style_name> page_break_base(const pugi::xml_node &n)
{
    const std::string_view name = n.attribute("style:name").as_string();
    std::optional<para_style_name> base;
    if(name == page_break_only || name == page_break_suffix.substr(1))
        base = para_style_name{};
    else if(name.size() > page_break_suffix.size() && name.ends_with(page_break_suffix))
        base = para_style_name(
            odf::model_name(name.substr(0, name.size() - page_break_suffix.size())));
    if(!base || std::string_view(n.name()) != "style:style")
        return std::nullopt;

    const auto s = read_style(n);
    const auto *ps = s ? std::get_if<paragraph_style>(&*s) : nullptr;
    if(!ps || ps->para.m_break_before != break_before{break_type::page})
        return std::nullopt;
    return base;
}

std::optional<odf_style> read_style(const pugi::xml_node &n)
{
    const auto family = from_odf(family_names, n.attribute("style:family").as_string());
    if(!family)
        return std::nullopt;

    const std::string name = odf::model_name(n.attribute("style:name").as_string());
    const std::string parent = odf::model_name(n.attribute("style:parent-style-name").as_string());
    auto para = [&]
    {
        return read_paragraph_props(n.child("style:paragraph-properties"))
            .value_or(paragraph_props{});
    };
    auto text = [&]
    { return read_text_props(n.child("style:text-properties")).value_or(text_props{}); };
    switch(*family)
    {
    case style_family::para:
    {
        paragraph_style s(name, para(), text());
        s.parent = parent;
        return s;
    }
    case style_family::text:
    {
        text_style s(name, text());
        s.parent = parent;
        return s;
    }
    case style_family::table:
    {
        table_style s(
            name, read_table_props(n.child("style:table-properties")).value_or(table_props{}));
        s.parent = parent;
        return s;
    }
    case style_family::cell:
    {
        cell_style s(name,
            read_cell_props(n.child("style:table-cell-properties")).value_or(cell_props{}),
            para(),
            text());
        s.parent = parent;
        return s;
    }
    case style_family::row: return row_style(name);
    case style_family::frame: return frame_style(name);
    case style_family::list: break;
    }
    return std::nullopt;
}

list_style read_list_style(const pugi::xml_node &n)
{
    list_style ls(odf::model_name(n.attribute("style:name").as_string()));
    for(auto level : n.children())
    {
        const std::string_view tag = level.name();
        const int lvl = level.attribute("text:level").as_int(1);
        const text_style_name sn(odf::model_name(level.attribute("text:style-name").as_string()));
        if(tag == "text:list-level-style-number")
        {
            const auto fmt = list_enum_from(level.attribute("style:num-format").as_string());
            if(!fmt)
                continue;
            ls.levels.push_back(number_level{.level = lvl,
                .format = *fmt,
                .prefix = level.attribute("style:num-prefix").as_string(),
                .suffix = level.attribute("style:num-suffix").as_string(),
                .start_from = level.attribute("text:start-value").as_int(1),
                .style = sn});
        }
        else if(tag == "text:list-level-style-bullet")
            ls.levels.push_back(bullet_level{.level = lvl,
                .bullet = level.attribute("text:bullet-char").as_string(),
                .style = sn});
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
