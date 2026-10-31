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
#include <filesystem>
#include <fstream>
#include <sstream>
#include <variant>

#include <fmt/format.h>

#include "docsmithcpp/odt/writer.h"
#include "docsmithcpp/text_doc.h"

namespace docsmith::odt
{
namespace lzpp = libzippp;
namespace fs = std::filesystem;

std::string get_media_type(const fs::path &p)
{
    if(p.extension() == ".png")
        return "image/png";
    return {};
}

std::string to_string(align_horiz a);
std::string to_string(align_vert a);
std::string to_string(font_style fs);
std::string to_string(font_weight fw);
std::string to_string(text_align ta);
std::string to_string(break_type b);

writer::writer()
{
    std::string odt_base_content{R"~~(
<?xml version="1.0" encoding="UTF-8"?>
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
    office:version="1.2">
    <office:styles>
    </office:styles>
    <office:automatic-styles>
    </office:automatic-styles>
    <office:body>
    </office:body>
</office:document-content>
)~~"};

    m_content.load_string(odt_base_content.c_str());

    auto find_node = [this](std::string name)
    {
        return m_content.find_node([name](const pugi::xml_node &node) { //
            return std::string(node.name()) == name;
        });
    };

    m_body = find_node("office:body");
    m_automatic_styles = find_node("office:automatic-styles");
    m_styles = find_node("office:styles");

    std::string manifest_xml{R"(
<?xml version="1.0" encoding="UTF-8"?>
<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" manifest:version="1.2">
  <manifest:file-entry manifest:media-type="application/vnd.oasis.opendocument.text" manifest:full-path="/"/>
  <manifest:file-entry manifest:media-type="text/xml" manifest:full-path="content.xml"/>
</manifest:manifest>)"};

    m_manifest.load_string(manifest_xml.c_str());
    m_manifest_files_node = m_manifest.root().child("manifest:manifest");
}

void writer::write(const text_doc &doc, const std::string &filename)
{
    writer w;

    fs::path parent_path = fs::path(filename).parent_path();
    if(!parent_path.empty() && !fs::exists(parent_path))
        fs::create_directories(parent_path);

    w.write_doc(doc);

    lzpp::ZipArchive zip(filename);
    if(!zip.open(lzpp::ZipArchive::New))
        throw std::runtime_error("Unable to create odt zip archive for writing");

    // Add the mimetype, must be uncompressed and first:
    zip.setCompressionMethod(lzpp::CompressionMethod::STORE);
    std::string mimetype{"application/vnd.oasis.opendocument.text"};
    if(!zip.addData("mimetype", mimetype.data(), mimetype.length()))
        throw std::runtime_error("Could not create mimetype");

    // Add content.xml:
    std::ostringstream content_stream;
    w.m_content.print(content_stream, "  ");
    std::string content_xml = content_stream.str();
    zip.addData("content.xml", content_xml.data(), content_xml.size());

    // Also save in a file for easy debugging:
    std::ofstream content_file("content.xml");
    content_file << content_xml;
    content_file.close();

    // Add the pictures and their manifest entries:
    for(auto &picture : w.m_pictures)
    {
        if(!zip.addFile(picture.m_dest, picture.m_source))
            throw std::runtime_error("Could not add file to archive");
        auto manifest_entry = w.m_manifest_files_node.append_child("manifest:file-entry");
        manifest_entry.append_attribute("manifest:full-path").set_value(picture.m_dest);
        manifest_entry.append_attribute("manifest:media-type").set_value(picture.m_type);
    }
    std::ostringstream manifest_stream;
    w.m_manifest.print(manifest_stream);
    std::string manifest_xml = manifest_stream.str();
    zip.addData("META-INF/manifest.xml", manifest_xml.data(), manifest_xml.size());

    zip.close();
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
    n.append_attribute("xlink:href").set_value(h.url.c_str());
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
    auto n = parent.append_child("draw:image");

    archive_item entry{};
    entry.m_dest = ("Pictures" / fs::path(v.uri).filename()).generic_string();
    entry.m_source = v.uri;
    entry.m_type = get_media_type(v.uri);
    m_pictures.insert(entry);

    n.append_attribute("xlink:href").set_value(entry.m_dest.c_str());
    n.append_attribute("xlink:type").set_value("simple");
    n.append_attribute("xlink:show").set_value("embed");
    n.append_attribute("draw:mime-type").set_value(entry.m_type.c_str());
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
