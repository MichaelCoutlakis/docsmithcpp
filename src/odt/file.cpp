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
#include "docsmithcpp/odt/file.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <pugixml.hpp>

#include "odf/meta.h"
#include "odf/package.h"
#include "odt/writer.h"

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

constexpr const char *odt_mimetype = "application/vnd.oasis.opendocument.text";

} // namespace

struct file::impl
{
    odf::package pkg; //!< Everything in the archive; empty for a new document
};

file::file() :
    m_impl(impl{odf::package(odt_mimetype)})
{
}

file::file(text_doc d) :
    doc(std::move(d)), m_impl(impl{odf::package(odt_mimetype)})
{
}

file file::load(const std::filesystem::path &p)
{
    file f;
    f.m_impl->pkg = odf::package::read(p);

    // Text documents and their relatives (.ott templates) share this prefix.
    if(!f.m_impl->pkg.mimetype().starts_with(odt_mimetype))
        throw std::runtime_error("Not an OpenDocument text file: " + p.string());

    const auto *content = f.m_impl->pkg.find("content.xml");
    pugi::xml_document xml;
    if(!content || !xml.load_buffer(content->data.data(), content->data.size()))
        throw std::runtime_error("Could not read content.xml from " + p.string());

    auto text_node =
        xml.child("office:document-content").child("office:body").child("office:text");
    f.doc.body = parse_block_children(text_node);
    if(const auto *meta = f.m_impl->pkg.find("meta.xml"))
        f.doc.meta = odf::read_meta(meta->data);

    // Move the images the tree refers to out of the package into the document,
    // so every format can reach them. Entries only unknown markup refers to
    // stay in the package and are carried over.
    for(const frame *fr : collect<frame>(f.doc))
        if(auto e = f.m_impl->pkg.take(fr->img.uri))
            f.doc.resources().add(
                resource(fr->img.uri, std::move(e->data), std::move(e->media_type)));
    return f;
}

void file::save(const std::filesystem::path &p) const
{
    // Work on a copy so saving leaves this file unchanged and can be repeated.
    odf::package out = m_impl->pkg;
    out.set_mimetype(odt_mimetype);

    // Written before content.xml is replaced: the writer reads the original.
    std::string content = write_content(doc, out);
    out.set("content.xml", std::move(content), "text/xml");
    odf::write_meta(doc.meta, out);
    out.write(p);

#ifdef DOCSMITHCPP_DEBUG_FILES
    // Debug aid: content.xml next to the archive, as <name>.content.xml.
    std::ofstream(std::filesystem::path(p).replace_extension(".content.xml"), std::ios::binary)
        << out.find("content.xml")->data;
#endif
}

} // namespace odt
} // namespace docsmith
