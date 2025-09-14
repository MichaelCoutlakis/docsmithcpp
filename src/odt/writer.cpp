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
#include <fmt/format.h>
#include <iostream>
#include <sstream>

#include "docsmithcpp/iostream_writer.h"
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

writer::writer(std::string filename)
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

    auto office_body = find_node("office:body");
    m_node_stack.push(office_body);
    m_current = m_node_stack.top();
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
    writer w(filename);

    fs::path parent_path = fs::path(filename).parent_path();
    if(!parent_path.empty() && !fs::exists(parent_path))
        fs::create_directories(parent_path);

    doc.accept(w);
    std::string content_xml, manifest_xml;

    lzpp::ZipArchive zip(filename);
    if(!zip.open(lzpp::ZipArchive::New))
        throw std::runtime_error("Unable to create odt zip archive for writing");

    // Add the mimetype, must be uncompressed and first:
    zip.setCompressionMethod(lzpp::CompressionMethod::STORE);
    std::string m_mimetype = std::string{"application/vnd.oasis.opendocument.text"};
    if(!zip.addData("mimetype", m_mimetype.data(), m_mimetype.length()))
        throw std::runtime_error("Could not create mimetype");

    // Add the content.xml:
    std::ostringstream content_stream;
    w.m_content.print(content_stream, "  ");
    content_xml = content_stream.str();
    zip.addData("content.xml", content_xml.data(), content_xml.size());

    // Add the manifest:

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
    w.m_manifest.print(std::cout, "  ");
    manifest_xml = manifest_stream.str();
    zip.addData("META-INF/manifest.xml", manifest_xml.data(), manifest_xml.size());

    zip.close();
}

void writer::visit(const text &val)
{
    get_current().append_child(pugi::node_pcdata).set_value(val.m_text);
}

void writer::visit(const span &) { get_current().append_child("text:span"); }

void writer::visit(const heading &) { get_current().append_child("text:h"); }

void writer::visit(const paragraph &p)
{
    auto n = get_current().append_child("text:p");
    n.append_attribute("text:style-name").set_value(p.get_style().get_name().c_str());
}

void writer::visit(const hyperlink &v)
{
    auto url = get_current().append_child("text:a");
    url.append_attribute("xlink:href") = v.get_url();
}

void writer::visit(const text_doc &doc)
{
    get_current().append_child("office:text");

    // Add any list / automatic styles:
    for(auto &[name, s] : doc.list_styles())
        write_xml(m_automatic_styles, s);
    // Add any text styles:
    for(auto &[name, s] : doc.styles())
        write_xml(m_styles, s);
}

void writer::visit(const list &l)
{
    auto n = get_current().append_child("text:list");
    n.append_attribute("text:style-name").set_value(l.get_style().get_name().c_str());
}

void writer::visit(const list_item &) { auto n = get_current().append_child("text:list-item"); }

void writer::visit(const list_style_num &) { get_current().append_child(""); }

void writer::visit(const list_style_bullet &) { get_current().append_child(""); }

void writer::visit(const frame &) { get_current().append_child("draw:frame"); }

void writer::visit(const bookmark &b)
{
    auto n = get_current().append_child("text:bookmark");
    n.append_attribute("text:name").set_value(b.m_name.c_str());
}

void writer::visit(const image &v)
{
    auto n = get_current().append_child("draw:image");

    // TODO: How to map the uri to archive / ODT filename?
    // Add the image to the manifest and mark an entry for archival:
    archive_item entry{};
    entry.m_dest = ("Pictures" / fs::path(v.get_uri()).filename()).generic_string();
    entry.m_source = v.get_uri();
    entry.m_type = get_media_type(v.get_uri());
    m_pictures.insert(entry);
    n.append_attribute("xlink:href").set_value(entry.m_dest);
    n.append_attribute("xlink:type").set_value("simple");
    n.append_attribute("xlink:show").set_value("embed");
    n.append_attribute("draw:mime-type").set_value(entry.m_type);
    n.append_attribute("xlink:actuate").set_value("onLoad");
}

void writer::push() { m_node_stack.push(get_current().last_child()); }

void writer::pop() { m_node_stack.pop(); }

pugi::xml_node writer::get_current() { return m_node_stack.top(); }

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
    {
        std::visit([&](const auto &s) { write_xml(ls_node, s); }, level_style);
    }
}

void write_xml(pugi::xml_node &n, const list_style_bullet &lsb)
{
    auto bullet_node = n.append_child("text:list-level-style-bullet");
    // write_xml(bullet_node, lsb.m_style_name);
    //   TODO: type parameter for write_xml with the style name:
    if(!lsb.m_style_name.is_empty())
        n.append_attribute("text:style-name").set_value(lsb.m_style_name.get_name().c_str());
    bullet_node.append_attribute("text:level") = lsb.m_level;
    bullet_node.append_attribute("text:bullet-char") = lsb.m_bullet_char.c_str();
    // TODO: serialize optional props

    // auto llp = bullet_node.append_child("style:list-level-properties");
    // llp.append_attribute("text:list-level-position-and-space-mode") = "label-alignment";
    // auto llpa = llp.append_child("style:list-level-label-alignment");
    // llpa.append_attribute("text:label-followed-by") = "listtab";
    // llpa.append_attribute("text:list-tab-stop-position") = "1.27cm";
    // llpa.append_attribute("fo:text-indent") = "-0.635cm";
    // llpa.append_attribute("fo:margin-left") = "1.27cm";
}

void write_xml(pugi::xml_node &n, const list_style_num &s)
{
    auto num_node = n.append_child("text:list-level-style-number");
    // write_xml(num_node, s.m_style_name);

    // TODO: type parameter for write_xml with the style name:
    if(!s.m_style_name.is_empty())
        n.append_attribute("text:style-name").set_value(s.m_style_name.get_name().c_str());

    // if(!s.m_style_name.is_empty())

    num_node.append_attribute("text:level") = s.m_level;
    num_node.append_attribute("loext:num-list-format") = fmt::format("%{}%.", s.m_level);
    num_node.append_attribute("style:num-format") =
        fmt::format("{}", static_cast<char>(s.m_format));
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
            fmt::format("{}pt", tp.m_font_size->m_points);

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
}
