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
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stack>

#include <fmt/format.h>
#include <libzippp/libzippp.h>
#include <pugixml.hpp>

#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/odt/writer.h"
#include "docsmithcpp/parser.h"
namespace docsmith
{
using block_factory =
    std::map<std::string, std::function<std::unique_ptr<element>(pugi::xml_node &node)>>;

namespace odt
{

hyperlink make_hyperlink(pugi::xml_node &node)
{
    style_name sn(node.attribute("text:style-name").as_string());
    std::string url(node.attribute("xlink:href").as_string());
    return hyperlink(url);
}

span make_span(pugi::xml_node &node)
{
    style_name sn(node.attribute("text:style-name").as_string());
    return span{}.set_style(sn);
}

paragraph make_paragraph(pugi::xml_node &node)
{
    style_name sn(node.attribute("text:style-name").as_string());
    return paragraph{}.set_style(sn);
}

heading make_heading(pugi::xml_node &node)
{
    int level = node.attribute("text:outline-level").as_int();
    style_name sn(node.attribute("text:style-name").as_string());
    return heading(level);
}

list make_list(pugi::xml_node &node)
{
    style_name sn(node.attribute("text:style-name").as_string());
    return list().set_style(sn);
}

list_item make_list_item(pugi::xml_node &node) { return list_item(); }

frame make_frame(pugi::xml_node &node)
{
    style_name sn(node.attribute("draw:style-name").as_string());
    return frame().set_style(sn);
}

image make_image(pugi::xml_node &node)
{

    image i(node.attribute("xlink:href").as_string());
    return i;
}

table make_table(pugi::xml_node &node)
{
    table t;
    return t;
}

table_cell make_table_cell(pugi::xml_node &node)
{
    table_cell c;
    c.rowspan() = node.attribute("table:number-rows-spanned").as_ullong(1U);
    c.colspan() = node.attribute("table:number-columns-spanned").as_ullong(1U);

    return c;
}

template <typename T>
std::function<std::unique_ptr<element>(pugi::xml_node &)> wrap_factory(
    std::function<T(pugi::xml_node &)> make_object)
{
    return [make_object](pugi::xml_node &node) { return std::make_unique<T>(make_object(node)); };
}

block_factory factory = {
    {"text:p", wrap_factory<paragraph>(make_paragraph)},
    {"text:h", wrap_factory<heading>(make_heading)},
    {"text:span", wrap_factory<span>(make_span)},
    {"text:a", wrap_factory<hyperlink>(make_hyperlink)},
    {"text:list", wrap_factory<list>(make_list)},
    {"text:list-item", wrap_factory<list_item>(make_list_item)},
    {"draw:frame", wrap_factory<frame>(make_frame)},
    {"draw:image", wrap_factory<image>(make_image)},
    {"table:table", wrap_factory<table>(make_table)},
    {"table:table-cell", wrap_factory<table_cell>(make_table_cell)},
};
}

odt_file::odt_file(const std::string &filename) :
    m_filename(filename)
{
}
const char *node_types[] = {
    "null", "document", "element", "pcdata", "cdata", "comment", "pi", "declaration"};

struct parser
{
    parser(const block_factory &factory) :
        m_factory(factory)
    {
    }
    void traverse(pugi::xml_node &node)
    {
        for(auto &child : node.children())
        {
            handle_node(child);
        }
    }
    text_doc get() { return m_doc; }

private:
    void handle_node(pugi::xml_node &node)
    {
        auto tag = std::string(node.name());
        std::cout << "Processing tag " << tag << "\n";

        if(auto it = m_factory.find(tag); it != m_factory.end())
        {
            m_blocks.push(context{it->second(node)});
            handle_child_nodes(node);

            // This level is now complete:
            complete_level();
            // auto completed_ctx = std::move(ctx());
            // m_blocks.pop();
            // if(!m_blocks.empty())
            //{
            //     if(current()->is_type(elem_t::tbl))
            //         add_to_table(std::move(completed_ctx.m_elem));
            //     else
            //         current()->add_child(std::move(completed_ctx.m_elem));
            // }
            // else
            //     m_doc.add_child(std::move(completed_ctx.m_elem));
        }
        else if(handle_table_nodes(node))
            std::cout << "table node handled\n";
        else
            std::cout << "Tag " << tag << " is not handled\n";
    }

    void handle_child_nodes(pugi::xml_node &node)
    {
        for(auto child : node.children())
        {
            if(child.type() == pugi::node_pcdata)
            {
                m_blocks.top().m_elem->add_child(
                    std::make_unique<text>(std::string(child.value())));
            }
            else
            {
                // Recursion:
                handle_node(child);
            }
        }
    }

    bool handle_table_nodes(pugi::xml_node &node)
    {
        if(!has_ctx() || !current()->is_type(elem_t::tbl))
            return false;

        // We are in a table, check for table elements like table:table-header-rows
        auto tag = std::string(node.name());
        auto t = current()->as<table>();

        if(tag == "table:table-header-rows")
            ctx().m_tbl_ctx.m_num_header_rows++;
        else if(tag == "table:table-row")
            // ctx().m_tbl_ctx.m_row_idx++;
            t->add_row();
        else
            return false;

        handle_child_nodes(node);
        return true;
        // else if(tag == "
    }

    void complete_level()
    {
        auto completed_ctx = std::move(ctx());
        m_blocks.pop();
        if(!m_blocks.empty())
        {
            // Handle table cells differently
            if(current()->is_type(elem_t::tbl) && completed_ctx.m_elem->is_type(elem_t::cell))
            {
                auto t = current()->as<table>();
                auto cell = dynamic_cast<table_cell *>(completed_ctx.m_elem.release());
                if(!cell)
                    throw std::runtime_error("parse table / cell logic broken");

                t->add_cell(std::move(*cell));
            }
            else
                current()->add_child(std::move(completed_ctx.m_elem));
        }
        else
            m_doc.add_child(std::move(completed_ctx.m_elem));
    }

    struct table_context
    {
        size_t m_num_header_rows{};
        size_t m_row_idx{};
        // size_t m_col_idx{};
    };

    /// Additional parser context:
    struct context
    {
        std::unique_ptr<element> m_elem;
        table_context m_tbl_ctx{};
    };

    context &ctx() { return m_blocks.top(); }
    bool has_ctx() { return !m_blocks.empty(); }

    std::unique_ptr<element> &current() { return ctx().m_elem; }

    block_factory m_factory;
    // std::stack<std::unique_ptr<element>> m_blocks;
    std::stack<context> m_blocks;
    text_doc m_doc;
};

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

    auto doc_node = xml.child("office:document-content").child("office:body").child("office:text");

    doc_node.children();

    parser p(odt::factory);
    p.traverse(doc_node);

    return p.get();
}

void odt_file::save(const text_doc &doc) { odt::writer::write(doc, m_filename); }
}