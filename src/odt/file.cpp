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
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <pugixml.hpp>

#include "odf/meta.h"
#include "odf/package.h"
#include "odt/styles.h"
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
        const std::string href = node.attribute("xlink:href").as_string();
        hyperlink h = href.starts_with('#') ? hyperlink(bookmark_target{href.substr(1)})
                                            : hyperlink(url_target{href});
        h.children() = parse_inline_children(node);
        return inline_node{std::move(h)};
    }
    if(tag == "draw:frame")
        return inline_node{parse_frame(node)};
    if(tag == "text:bookmark" || tag == "text:bookmark-start")
        return inline_node{bookmark{node.attribute("text:name").as_string()}};
    if(tag == "text:page-number")
        return inline_node{field{field_type::page_number}};
    if(tag == "text:page-count")
        return inline_node{field{field_type::page_count}};
    if(tag == "text:date")
        return inline_node{field{field_type::date}};
    if(tag == "text:title")
        return inline_node{field{field_type::title}};

    return inline_node{unknown{serialize(node)}};
}

/// Appends s to the last node if that is text, so that ODF's space, tab and
/// line break elements join the text around them.
void append_text(std::vector<inline_node> &out, const std::string &s)
{
    if(!out.empty())
        if(auto *t = std::get_if<text>(&out.back().v))
        {
            t->content += s;
            return;
        }
    out.push_back(inline_node{text{s}});
}

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

/// Text as ODF reads it: each run of white space is one space, and white
/// space at the start of a paragraph or heading is dropped.
std::string collapse_space(std::string_view s, bool at_start)
{
    std::string out;
    bool space = at_start;
    for(char c : s)
    {
        if(is_space(c))
        {
            if(!space)
                out += ' ';
            space = true;
        }
        else
        {
            out += c;
            space = false;
        }
    }
    return out;
}

std::vector<inline_node> parse_inline_children(const pugi::xml_node &node)
{
    const std::string_view tag = node.name();
    const bool paragraph = tag == "text:p" || tag == "text:h";

    std::vector<inline_node> out;
    for(auto child : node.children())
    {
        const std::string_view name = child.name();
        if(child.type() == pugi::node_pcdata)
        {
            const bool at_start = paragraph && child == node.first_child();
            if(auto s = collapse_space(child.value(), at_start); !s.empty())
                append_text(out, s);
        }
        else if(child.type() != pugi::node_element)
            continue;
        else if(name == "text:s")
            append_text(out, std::string(child.attribute("text:c").as_uint(1), ' '));
        else if(name == "text:tab")
            append_text(out, "\t");
        else if(name == "text:line-break")
            append_text(out, "\n");
        else
            out.push_back(parse_inline(child));
    }
    return out;
}

std::vector<table_cell> parse_row(const pugi::xml_node &row_node)
{
    std::vector<table_cell> row;
    for(auto cell_node : row_node.children())
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
    return row;
}

table parse_table(const pugi::xml_node &node)
{
    table t;
    for(auto child : node.children())
    {
        std::string tag = child.name();
        if(tag == "table:table-header-rows")
        {
            for(auto row : child.children("table:table-row"))
                t.rows.push_back(parse_row(row));
            // The model has header rows only at the top; later ones are ordinary.
            if(t.header_rows == 0 && t.rows.size() == child.select_nodes("table:table-row").size())
                t.header_rows = t.rows.size();
        }
        else if(tag == "table:table-row")
            t.rows.push_back(parse_row(child));
    }
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

/// Undoes the writer's folding of page_break nodes into the next paragraph or
/// heading (see writer::page_break_style()). styles: the page break styles in
/// content.xml, and the style each was made from.
void expand_page_breaks(
    std::vector<block_node> &blocks, const std::map<std::string, style_name> &styles)
{
    std::vector<block_node> out;
    for(auto &b : blocks)
    {
        bool drop = false;
        std::visit(
            overloaded{
                [&](list &l)
                {
                    for(auto &it : l.items())
                        expand_page_breaks(it.children(), styles);
                    // The writer folds a break before a list into its
                    // first paragraph; it reads back before the list.
                    if(auto &items = l.items(); !items.empty() &&
                        !items.front().children().empty() &&
                        std::holds_alternative<page_break>(items.front().children().front().v))
                    {
                        items.front().children().erase(items.front().children().begin());
                        out.push_back(block_node{page_break{}});
                    }
                },
                [&](table &t)
                {
                    for(auto &row : t.rows)
                        for(auto &c : row)
                            expand_page_breaks(c.children(), styles);
                },
                [&](auto &n)
                {
                    using T = std::decay_t<decltype(n)>;
                    if constexpr(std::is_same_v<T, paragraph> || std::is_same_v<T, heading>)
                    {
                        auto it = styles.find(n.style.get_name());
                        if(it == styles.end())
                            return;
                        out.push_back(block_node{page_break{}});
                        drop = it->first == page_break_only;
                        n.style = it->second;
                    }
                },
            },
            b.v);
        if(!drop)
            out.push_back(std::move(b));
    }
    blocks = std::move(out);
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
    // White space between inline elements is a space, so keep it.
    if(!content ||
        !xml.load_buffer(content->data.data(),
            content->data.size(),
            pugi::parse_default | pugi::parse_ws_pcdata))
        throw std::runtime_error("Could not read content.xml from " + p.string());

    auto root = xml.child("office:document-content");
    f.doc.body = parse_block_children(root.child("office:body").child("office:text"));

    std::map<std::string, style_name> page_breaks;
    for(auto s : root.child("office:automatic-styles").children("style:style"))
        if(auto base = page_break_base(s))
            page_breaks[s.attribute("style:name").as_string()] = *base;
    if(!page_breaks.empty())
        expand_page_breaks(f.doc.body, page_breaks);
    read_styles(f.m_impl->pkg, f.doc);
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

    write_document(doc, out);
    odf::write_meta(doc.meta, out);
    out.write(p);

#ifdef DOCSMITHCPP_DEBUG_FILES
    // Debug aid: content.xml and styles.xml next to the archive, as
    // <name>.content.xml and <name>.styles.xml.
    // Indented for reading; the package's copies aren't, as indentation between
    // inline elements would read as spaces.
    for(const char *part : {"content", "styles"})
    {
        const auto &data = out.find(std::string(part) + ".xml")->data;
        pugi::xml_document xml;
        xml.load_buffer(data.data(), data.size(), pugi::parse_default | pugi::parse_ws_pcdata);
        xml.save_file(
            (std::filesystem::path(p).replace_extension(std::string(".") + part + ".xml")).c_str(),
            "  ");
    }
#endif
}

} // namespace odt
} // namespace docsmith
