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
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <libzippp/libzippp.h>
#include <pugixml.hpp>

#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/odt/writer.h"

namespace docsmith
{
namespace odt
{
namespace
{

/// Serialize an XML subtree verbatim so it can be re-emitted unchanged.
std::string serialize(const pugi::xml_node &node)
{
    std::ostringstream ss;
    node.print(ss, "", pugi::format_raw);
    return ss.str();
}

style_name style_of(const pugi::xml_node &node, const char *attr)
{
    return style_name(node.attribute(attr).as_string());
}

std::vector<inline_node> parse_inline_children(const pugi::xml_node &node);
std::vector<block_node> parse_block_children(const pugi::xml_node &node);
std::optional<block_node> parse_block(const pugi::xml_node &node);

image parse_image(const pugi::xml_node &node) { return image{node.attribute("xlink:href").as_string()}; }

frame parse_frame(const pugi::xml_node &node)
{
    frame f;
    f.style = style_of(node, "draw:style-name");
    if(auto img = node.child("draw:image"))
        f.img = parse_image(img);
    return f;
}

inline_node parse_inline(const pugi::xml_node &node)
{
    std::string tag = node.name();

    if(tag == "text:span")
    {
        span s;
        s.style = style_of(node, "text:style-name");
        s.children() = parse_inline_children(node);
        return inline_node{std::move(s)};
    }
    if(tag == "text:a")
    {
        hyperlink h(node.attribute("xlink:href").as_string());
        h.children() = parse_inline_children(node);
        return inline_node{std::move(h)};
    }
    if(tag == "draw:frame")
        return inline_node{parse_frame(node)};
    if(tag == "text:bookmark" || tag == "text:bookmark-start")
        return inline_node{bookmark{node.attribute("text:name").as_string()}};

    return inline_node{unknown{serialize(node)}};
}

std::vector<inline_node> parse_inline_children(const pugi::xml_node &node)
{
    std::vector<inline_node> out;
    for(auto child : node.children())
    {
        if(child.type() == pugi::node_pcdata)
            out.push_back(inline_node{text{child.value()}});
        else if(child.type() == pugi::node_element)
            out.push_back(parse_inline(child));
    }
    return out;
}

table parse_table(const pugi::xml_node &node)
{
    table t;

    auto parse_rows_in = [&t](const pugi::xml_node &container, auto &&self) -> void
    {
        for(auto child : container.children())
        {
            std::string tag = child.name();
            if(tag == "table:table-header-rows")
                self(child, self); // flatten header rows into the body
            else if(tag == "table:table-row")
            {
                std::vector<table_cell> row;
                for(auto cell_node : child.children())
                {
                    std::string ct = cell_node.name();
                    if(ct == "table:table-cell")
                    {
                        table_cell c;
                        c.rowspan = cell_node.attribute("table:number-rows-spanned").as_int(1);
                        c.colspan = cell_node.attribute("table:number-columns-spanned").as_int(1);
                        c.style = style_of(cell_node, "table:style-name");
                        c.children() = parse_block_children(cell_node);
                        row.push_back(std::move(c));
                    }
                    else if(ct == "table:covered-table-cell")
                        row.push_back(table_cell{}); // placeholder for a spanned-over slot
                }
                t.rows.push_back(std::move(row));
            }
        }
    };
    parse_rows_in(node, parse_rows_in);
    return t;
}

list parse_list(const pugi::xml_node &node)
{
    list l;
    l.style = style_of(node, "text:style-name");
    for(auto item_node : node.children("text:list-item"))
    {
        list_item it;
        it.children() = parse_block_children(item_node);
        l.items().push_back(std::move(it));
    }
    return l;
}

std::optional<block_node> parse_block(const pugi::xml_node &node)
{
    std::string tag = node.name();

    if(tag == "text:p")
    {
        paragraph p;
        p.style = style_of(node, "text:style-name");
        p.children() = parse_inline_children(node);
        return block_node{std::move(p)};
    }
    if(tag == "text:h")
    {
        heading h(node.attribute("text:outline-level").as_int(1));
        h.style = style_of(node, "text:style-name");
        h.children() = parse_inline_children(node);
        return block_node{std::move(h)};
    }
    if(tag == "text:list")
        return block_node{parse_list(node)};
    if(tag == "table:table")
        return block_node{parse_table(node)};
    if(tag == "draw:frame")
        return block_node{parse_frame(node)};

    return block_node{unknown{serialize(node)}};
}

std::vector<block_node> parse_block_children(const pugi::xml_node &node)
{
    std::vector<block_node> out;
    for(auto child : node.children())
        if(child.type() == pugi::node_element)
            if(auto b = parse_block(child))
                out.push_back(std::move(*b));
    return out;
}

} // namespace
} // namespace odt

odt_file::odt_file(const std::string &filename) :
    m_filename(filename)
{
}

text_doc odt_file::parse_text_doc()
{
    libzippp::ZipArchive zip(m_filename);
    if(!zip.open(libzippp::ZipArchive::ReadOnly))
        throw std::runtime_error("Could not open archive");

    auto content = zip.getEntry("content.xml");
    if(content.isNull())
        throw std::runtime_error("Could not open content.xml");

    auto content_xml = content.readAsText();

    pugi::xml_document xml;
    if(!xml.load_string(content_xml.c_str()))
        throw std::runtime_error("Failed to parse the content.xml");

    auto text_node =
        xml.child("office:document-content").child("office:body").child("office:text");

    text_doc doc;
    doc.body = odt::parse_block_children(text_node);
    return doc;
}

void odt_file::save(const text_doc &doc) { odt::writer::write(doc, m_filename); }

} // namespace docsmith
