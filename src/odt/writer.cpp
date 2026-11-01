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
#include "odt/writer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <variant>

#include <fmt/format.h>

#include "odf/package.h"

namespace docsmith::odt
{
namespace fs = std::filesystem;

std::string to_string(align_horiz a);
std::string to_string(align_vert a);
std::string to_string(font_style fs);
std::string to_string(font_weight fw);
std::string to_string(text_align ta);
std::string to_string(break_type b);

namespace
{
std::string media_type_of(const fs::path &p)
{
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if(ext == ".png")
        return "image/png";
    if(ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if(ext == ".gif")
        return "image/gif";
    if(ext == ".svg")
        return "image/svg+xml";
    if(ext == ".bmp")
        return "image/bmp";
    return {};
}

std::string read_file(const fs::path &p)
{
    std::ifstream in(p, std::ios::binary);
    if(!in)
        throw std::runtime_error("Could not read file: " + p.string());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

constexpr const char *base_content = R"~~(<?xml version="1.0" encoding="UTF-8"?>
<office:document-content
    xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0"
    xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"
    xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
    xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
    xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0"
    xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:loext="urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0"
    office:version="1.2">
    <office:font-face-decls/>
    <office:styles/>
    <office:automatic-styles/>
    <office:body/>
</office:document-content>
)~~";

// LibreOffice's Title, its Heading parent's properties included.
constexpr const char *title_style = R"~~(<style:style style:name="Title" style:family="paragraph" style:class="chapter">
    <style:paragraph-properties fo:margin-top="0.423cm" fo:margin-bottom="0.212cm" fo:keep-with-next="always" fo:text-align="center"/>
    <style:text-properties fo:font-family="'Liberation Sans'" style:font-family-generic="swiss" fo:font-size="28pt" fo:font-weight="bold" style:font-size-asian="28pt" style:font-weight-asian="bold" style:font-size-complex="28pt" style:font-weight-complex="bold"/>
</style:style>)~~";

/// Whether the package's styles.xml (a loaded file's) defines the style.
bool defines_style(const odf::package &pkg, const style_name &name)
{
    const auto *styles = pkg.find("styles.xml");
    pugi::xml_document xml;
    if(!styles || !xml.load_buffer(styles->data.data(), styles->data.size()))
        return false;
    for(auto s : xml.child("office:document-styles").child("office:styles").children("style:style"))
        if(name.get_name() == s.attribute("style:name").as_string())
            return true;
    return false;
}
} // namespace

std::string write_content(const text_doc &doc, odf::package &pkg) { return writer(pkg).write(doc); }

writer::writer(odf::package &pkg) :
    m_pkg(pkg)
{
    m_content.load_string(base_content);
    auto root = m_content.child("office:document-content");
    root.attribute("office:version").set_value(m_pkg.version().c_str());
    m_font_faces = root.child("office:font-face-decls");
    m_styles = root.child("office:styles");
    m_automatic_styles = root.child("office:automatic-styles");
    m_body = root.child("office:body");
    carry_over();
}

std::string writer::write(const text_doc &doc)
{
    m_doc = &doc;
    write_doc(doc);
    std::ostringstream ss;
    m_content.save(ss, "  ");
    return ss.str();
}

// A loaded document's content.xml declares things the model doesn't hold: the
// namespaces its markup uses, its fonts, and its automatic styles (P1, T1, ...).
// Carry them over so the style names on parsed nodes, and any unknown nodes,
// still resolve after saving.
void writer::carry_over()
{
    const auto *original = m_pkg.find("content.xml");
    pugi::xml_document xml;
    if(!original || !xml.load_buffer(original->data.data(), original->data.size()))
        return;

    auto src = xml.child("office:document-content");
    auto dst = m_content.child("office:document-content");
    for(auto a : src.attributes())
        if(std::string_view(a.name()).starts_with("xmlns:") && !dst.attribute(a.name()))
            dst.append_attribute(a.name()) = a.value();

    for(auto c : src.child("office:font-face-decls").children())
        m_font_faces.append_copy(c);
    for(auto c : src.child("office:automatic-styles").children())
        m_automatic_styles.append_copy(c);
}

// Archive path for an image. A document resource is embedded under its name; an
// entry the package already holds is referenced in place; a file on disk is
// copied in under Pictures/, renamed if a different image already has that name.
std::string writer::resolve_image(const std::string &uri)
{
    if(const resource *r = m_doc->resources().find(uri))
    {
        if(const auto *e = m_pkg.find(uri); !e || e->data != r->data())
            m_pkg.set(uri, r->data(), r->media_type());
        return uri;
    }
    if(m_pkg.contains(uri))
        return uri;
    if(!fs::is_regular_file(uri))
        throw std::runtime_error("Image not found in the package or on disk: " + uri);

    std::string data = read_file(uri);
    const fs::path name = fs::path(uri).filename();
    std::string href = ("Pictures" / name).generic_string();
    for(int i = 2; m_pkg.contains(href) && m_pkg.find(href)->data != data; ++i)
        href = (fs::path("Pictures") /
            (name.stem().string() + "_" + std::to_string(i) + name.extension().string()))
                   .generic_string();

    if(!m_pkg.contains(href))
        m_pkg.set(href, std::move(data), media_type_of(uri));
    return href;
}

void writer::write_doc(const text_doc &doc)
{
    auto office_text = m_body.append_child("office:text");

    // Add any list / automatic styles:
    for(auto &[name, s] : doc.list_styles())
        write_xml(m_automatic_styles, s);
    // Add any text styles:
    for(auto &[name, s] : doc.styles())
        write_xml(m_styles, s);
    // LibreOffice knows Title without a definition; other readers may not.
    if(doc.find_title() && !doc.styles().find(builtin::title.get_name()) &&
        !defines_style(m_pkg, builtin::title))
        m_styles.append_buffer(title_style, std::strlen(title_style));

    for(const auto &b : doc.body)
        write_block(office_text, b);
}

void writer::write_block(pugi::xml_node parent, const block_node &b)
{
    std::visit([&](const auto &n) { emit(parent, n); }, b.v);
}

void writer::write_inline(pugi::xml_node parent, const inline_node &i)
{
    std::visit([&](const auto &n) { emit(parent, n); }, i.v);
}

void writer::emit(pugi::xml_node parent, const paragraph &p)
{
    auto n = parent.append_child("text:p");
    if(!p.style.is_empty())
        n.append_attribute("text:style-name").set_value(p.style.get_name().c_str());
    for(const auto &c : p.children())
        write_inline(n, c);
}

void writer::emit(pugi::xml_node parent, const heading &h)
{
    auto n = parent.append_child("text:h");
    n.append_attribute("text:outline-level").set_value(h.level);
    if(!h.style.is_empty())
        n.append_attribute("text:style-name").set_value(h.style.get_name().c_str());
    for(const auto &c : h.children())
        write_inline(n, c);
}

void writer::emit(pugi::xml_node parent, const span &s)
{
    auto n = parent.append_child("text:span");
    if(!s.style.is_empty())
        n.append_attribute("text:style-name").set_value(s.style.get_name().c_str());
    for(const auto &c : s.children())
        write_inline(n, c);
}

void writer::emit(pugi::xml_node parent, const hyperlink &h)
{
    auto n = parent.append_child("text:a");
    n.append_attribute("xlink:type").set_value("simple");
    const std::string href = std::visit(overloaded{[](const url_target &u) { return u.url; },
                                            [](const bookmark_target &b) { return "#" + b.name; }},
        h.target);
    n.append_attribute("xlink:href").set_value(href.c_str());
    for(const auto &c : h.children())
        write_inline(n, c);
}

void writer::emit(pugi::xml_node parent, const text &t)
{
    parent.append_child(pugi::node_pcdata).set_value(t.content.c_str());
}

void writer::emit(pugi::xml_node parent, const frame &f)
{
    auto n = parent.append_child("draw:frame");
    if(!f.style.is_empty())
        n.append_attribute("draw:style-name").set_value(f.style.get_name().c_str());
    emit(n, f.img);
}

void writer::emit(pugi::xml_node parent, const image &v)
{
    const std::string href = resolve_image(v.uri);

    auto n = parent.append_child("draw:image");
    n.append_attribute("xlink:href").set_value(href.c_str());
    n.append_attribute("xlink:type").set_value("simple");
    n.append_attribute("xlink:show").set_value("embed");
    n.append_attribute("draw:mime-type").set_value(m_pkg.find(href)->media_type.c_str());
    n.append_attribute("xlink:actuate").set_value("onLoad");
}

void writer::emit(pugi::xml_node parent, const bookmark &b)
{
    auto n = parent.append_child("text:bookmark");
    n.append_attribute("text:name").set_value(b.name.c_str());
}

void writer::emit(pugi::xml_node parent, const unknown &u)
{
    // Re-emit the captured subtree verbatim.
    pugi::xml_document frag;
    if(frag.load_string(u.xml.c_str()))
        for(auto child : frag.children())
            parent.append_copy(child);
}

void writer::emit(pugi::xml_node parent, const list &l)
{
    auto n = parent.append_child("text:list");
    if(!l.style.is_empty())
        n.append_attribute("text:style-name").set_value(l.style.get_name().c_str());
    for(const auto &it : l.items())
        emit(n, it);
}

void writer::emit(pugi::xml_node parent, const list_item &it)
{
    auto n = parent.append_child("text:list-item");
    for(const auto &b : it.children())
        write_block(n, b);
}

void writer::emit(pugi::xml_node parent, const table &t)
{
    auto n = parent.append_child("table:table");
    auto table_column = n.append_child("table:table-column");
    table_column.append_attribute("table:number-columns-repeated")
        .set_value(static_cast<unsigned>(t.num_cols()));

    for(size_t r = 0; r != t.num_rows(); ++r)
    {
        auto xml_row = n.append_child("table:table-row");
        for(size_t c = 0; c != t.num_cols(); ++c)
        {
            if(t.is_cell_covered(r, c))
                xml_row.append_child("table:covered-table-cell");
            else
                emit(xml_row, t.cell(r, c));
        }
    }
}

void writer::emit(pugi::xml_node parent, const table_cell &c)
{
    auto n = parent.append_child("table:table-cell");
    if(!c.style.is_empty())
        n.append_attribute("table:style-name").set_value(c.style.get_name().c_str());
    n.append_attribute("office:value-type").set_value("string");

    if(c.rowspan != 1)
        n.append_attribute("table:number-rows-spanned").set_value(c.rowspan);
    if(c.colspan != 1)
        n.append_attribute("table:number-columns-spanned").set_value(c.colspan);

    for(const auto &b : c.children())
        write_block(n, b);
}

void write_xml(pugi::xml_node &n, const style_name &sn)
{
    n.append_attribute("style:name").set_value(sn.get_name().c_str());
}

void write_xml(pugi::xml_node &n, const style &s)
{
    auto style_node = n.append_child("style:style");
    write_xml(style_node, s.m_name);

    // FIXME
    style_node.append_attribute("style:family") = "paragraph";

    if(!s.m_parent_style.is_empty())
        style_node.append_attribute("style:parent-style-name") =
            s.m_parent_style.get_name().c_str();

    if(s.m_text_props)
        write_xml(style_node, s.m_text_props.value());

    if(s.m_paragraph_props)
        write_xml(style_node, s.m_paragraph_props.value());

    // TODO: Other properties if present...
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
    if(!lsb.m_style_name.is_empty())
        n.append_attribute("text:style-name").set_value(lsb.m_style_name.get_name().c_str());
    bullet_node.append_attribute("text:level") = lsb.m_level;
    bullet_node.append_attribute("text:bullet-char") = lsb.m_bullet_char.c_str();
    // TODO: serialize optional props
}

void write_xml(pugi::xml_node &n, const list_style_num &s)
{
    auto num_node = n.append_child("text:list-level-style-number");
    if(!s.m_style_name.is_empty())
        n.append_attribute("text:style-name").set_value(s.m_style_name.get_name().c_str());

    num_node.append_attribute("text:level") = s.m_level;
    num_node.append_attribute("loext:num-list-format") = fmt::format("%{}%.", s.m_level).c_str();
    num_node.append_attribute("style:num-format") =
        fmt::format("{}", static_cast<char>(s.m_format)).c_str();
    num_node.append_attribute("text:start-value") = s.m_start_from;
    num_node.append_attribute("style:num-suffix") = s.m_num_suffix.c_str();
    num_node.append_attribute("style:num-prefix") = s.m_num_prefix.c_str();
    // TODO: serialize optional props
}
void write_xml(pugi::xml_node &n, const text_props &tp)
{
    auto text_props_node = n.append_child("style:text-properties");

    if(tp.m_font_size)
        text_props_node.append_attribute("fo:font-size") =
            fmt::format("{}pt", tp.m_font_size->m_points).c_str();

    if(tp.m_font_style)
        text_props_node.append_attribute("fo:font-style") =
            to_string(tp.m_font_style.value()).c_str();

    if(tp.m_font_name)
        text_props_node.append_attribute("style:font-name") = tp.m_font_name->get_name().c_str();

    if(tp.m_font_weight)
        text_props_node.append_attribute("fo:font-weight") =
            to_string(tp.m_font_weight.value()).c_str();

    if(tp.m_colour)
        text_props_node.append_attribute("fo:color") =
            fmt::format("#{:02x}{:02x}{:02x}", tp.m_colour->m_r, tp.m_colour->m_g, tp.m_colour->m_b)
                .c_str();
}
void write_xml(pugi::xml_node &n, const paragraph_props &props)
{
    auto para_props_node = n.append_child("style:paragraph-properties");

    if(props.m_break_after)
        para_props_node.append_attribute("fo:break-after") =
            to_string(props.m_break_after->m_break_type).c_str();
    if(props.m_break_before)
        para_props_node.append_attribute("fo:break-before") =
            to_string(props.m_break_before->m_break_type).c_str();
}

std::string to_string(align_horiz a)
{
    switch(a)
    {
    case align_horiz::left: return "left";
    case align_horiz::centre: return "center";
    case align_horiz::right: return "right";
    case align_horiz::from_inside: return "from-inside";
    case align_horiz::from_left: return "from-left";
    case align_horiz::inside: return "inside";
    case align_horiz::outside: return "outside";
    default: return {};
    }
}
std::string to_string(align_vert a)
{
    switch(a)
    {
    case align_vert::top: return "top";
    case align_vert::middle: return "middle";
    case align_vert::bottom: return "bottom";
    case align_vert::from_top: return "from-top";
    case align_vert::below: return "below";
    default: return {};
    }
}

std::string to_string(font_style fs)
{
    switch(fs)
    {
    case font_style::normal: return "normal";
    case font_style::italic: return "italic";
    case font_style::oblique: return "oblique";
    default: return {};
    }
}
std::string to_string(font_weight fw)
{
    switch(fw)
    {
    case font_weight::normal: return "normal";
    case font_weight::bold: return "bold";
    default: return {};
    }
}
std::string to_string(text_align ta)
{
    switch(ta)
    {
    case text_align::start: return "start";
    case text_align::end: return "end";
    case text_align::left: return "left";
    case text_align::right: return "right";
    case text_align::centre: return "center";
    case text_align::justify: return "justify";
    default: return {};
    }
}

std::string to_string(break_type b)
{
    switch(b)
    {
    case break_type::automatic: return "auto";
    case break_type::column: return "column";
    case break_type::page: return "page";
    case break_type::even_page: return "even-page";
    case break_type::odd_page: return "odd-page";
    default: return {};
    }
}
} // namespace docsmith::odt
