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
            tp.set(percent{pc});
        else if(auto w = length_from(n.attribute("style:width").as_string()))
            tp.set(*w);
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
        cp.set(*c);
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

namespace
{
/// The name shown to users: LibreOffice spells characters a name can't hold
/// as _xx_ in hex (Text_20_body is "Text body"). It finds its own styles by
/// that name, so a style named Heading_20_1 without it would be a new style.
std::string display_name(const std::string &name)
{
    auto hex = [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; };
    std::string out;
    for(size_t i = 0; i != name.size(); ++i)
    {
        if(name[i] == '_' && i + 3 < name.size() && hex(name[i + 1]) && hex(name[i + 2]) &&
            name[i + 3] == '_')
        {
            out += static_cast<char>(std::stoi(name.substr(i + 1, 2), nullptr, 16));
            i += 3;
        }
        else
            out += name[i];
    }
    return out;
}
} // namespace

void write_xml(pugi::xml_node &n, const style_name &sn)
{
    n.append_attribute("style:name").set_value(sn.get_name().c_str());
    if(const std::string shown = display_name(sn.get_name()); shown != sn.get_name())
        n.append_attribute("style:display-name").set_value(shown.c_str());
}

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

    // A table style's lines and header go on its cells (see the writer).
    if(family == style_family::table)
        write_xml(style_node, s.m_table_props.value_or(table_props{}));
    if(family == style_family::table_cell && s.m_cell_props)
        write_xml(style_node, *s.m_cell_props);

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

std::string page_break_style_name(const style_name &base)
{
    return base.is_empty() ? std::string(page_break_suffix.substr(1))
                           : base.get_name() + std::string(page_break_suffix);
}

std::optional<style_name> page_break_base(const pugi::xml_node &n)
{
    const std::string_view name = n.attribute("style:name").as_string();
    std::optional<style_name> base;
    if(name == page_break_only || name == page_break_suffix.substr(1))
        base = style_name{};
    else if(name.size() > page_break_suffix.size() && name.ends_with(page_break_suffix))
        base = style_name(std::string(name.substr(0, name.size() - page_break_suffix.size())));
    if(!base || std::string_view(n.name()) != "style:style")
        return std::nullopt;

    const auto s = read_style(n);
    if(!s || s->m_family != style_family::paragraph || !s->m_paragraph_props ||
        s->m_paragraph_props->m_break_before != break_before{break_type::page})
        return std::nullopt;
    return base;
}

std::optional<style> read_style(const pugi::xml_node &n)
{
    const auto family = from_odf(family_names, n.attribute("style:family").as_string());
    if(!family)
        return std::nullopt;

    style s(style_name(n.attribute("style:name").as_string()), *family);
    s.m_parent_style = style_name(n.attribute("style:parent-style-name").as_string());
    s.m_paragraph_props = read_paragraph_props(n.child("style:paragraph-properties"));
    s.m_text_props = read_text_props(n.child("style:text-properties"));
    if(*family == style_family::table)
        s.m_table_props = read_table_props(n.child("style:table-properties"));
    if(*family == style_family::table_cell)
        s.m_cell_props = read_cell_props(n.child("style:table-cell-properties"));
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
