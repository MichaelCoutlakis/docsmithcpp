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
    <office:automatic-styles>
        <text:list-style style:name="L2">
            <text:list-level-style-bullet text:level="1" text:style-name="Bullet_20_Symbols" loext:num-list-format="%1%." style:num-suffix="." text:bullet-char="•">
                <style:list-level-properties text:list-level-position-and-space-mode="label-alignment">
                    <style:list-level-label-alignment text:label-followed-by="listtab" text:list-tab-stop-position="1.27cm" fo:text-indent="-0.635cm" fo:margin-left="1.27cm"/>
                </style:list-level-properties>
            </text:list-level-style-bullet>
            <text:list-level-style-bullet text:level="2" text:style-name="Bullet_20_Symbols" loext:num-list-format="%2%." style:num-suffix="." text:bullet-char="◦">
                <style:list-level-properties text:list-level-position-and-space-mode="label-alignment">
                    <style:list-level-label-alignment text:label-followed-by="listtab" text:list-tab-stop-position="1.905cm" fo:text-indent="-0.635cm" fo:margin-left="1.905cm"/>
                </style:list-level-properties>
            </text:list-level-style-bullet>
        </text:list-style>
        <text:list-style style:name="L3">
            <text:list-level-style-number text:level="1" text:style-name="Numbering_20_Symbols" loext:num-list-format="%1%)" style:num-suffix=")" style:num-format="a" text:start-value="1">
                <style:list-level-properties text:list-level-position-and-space-mode="label-alignment">
                    <style:list-level-label-alignment text:label-followed-by="listtab" text:list-tab-stop-position="1.27cm" fo:text-indent="-0.635cm" fo:margin-left="1.27cm"/>
                </style:list-level-properties>
            </text:list-level-style-number>
            <text:list-level-style-number text:level="2" text:style-name="Numbering_20_Symbols" loext:num-list-format="%2%)" style:num-suffix=")" style:num-format="a">
                <style:list-level-properties text:list-level-position-and-space-mode="label-alignment">
                    <style:list-level-label-alignment text:label-followed-by="listtab" text:list-tab-stop-position="1.905cm" fo:text-indent="-0.635cm" fo:margin-left="1.905cm"/>
                </style:list-level-properties>
            </text:list-level-style-number>
        </text:list-style>
    </office:automatic-styles>
    <office:body>
    </office:body>
</office:document-content>
)~~"};

    m_content.load_string(odt_base_content.c_str());

    auto office_body = m_content.find_node(
        [](const pugi::xml_node &node) { return std::string(node.name()) == "office:body"; });
    m_node_stack.push(office_body);
    m_current = m_node_stack.top();

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

void writer::visit(const paragraph &) { get_current().append_child("text:p"); }

void writer::visit(const hyperlink &v)
{
    auto url = get_current().append_child("text:a");
    url.append_attribute("xlink:href") = v.get_url();
}

void writer::visit(const text_doc &) { get_current().append_child("office:text"); }

void writer::visit(const list &l)
{
    auto n = get_current().append_child("text:list");
    n.append_attribute("text:style-name").set_value(l.get_style().get_name().c_str());
}

void writer::visit(const list_item &) { auto n = get_current().append_child("text:list-item"); }

void writer::visit(const list_style_num &) { get_current().append_child(""); }

void writer::visit(const list_style_bullet &) { get_current().append_child(""); }

void writer::visit(const frame &) { get_current().append_child("draw:frame"); }

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
}