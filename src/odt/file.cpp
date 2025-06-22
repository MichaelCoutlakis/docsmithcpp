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
#include <stack>

#include <libzippp/libzippp.h>
#include <pugixml.hpp>

#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/parser.h"

namespace docsmith
{
using block_factory =
    std::map<std::string, std::function<std::unique_ptr<element>(pugi::xml_node &node)>>;

namespace odt
{
paragraph make_paragraph(pugi::xml_node &node) { return paragraph{}; }

heading make_heading(pugi::xml_node &node)
{
    int level = node.attribute("text:outline-level").as_int();
    return heading(level);
}

list make_list(pugi::xml_node &node) { return list{list::Marker::Bullet, 0}; }

list_item make_list_item(pugi::xml_node &node)
{
    // std::string s(node.text());

    return list_item();
}

template <typename T>
std::function<std::unique_ptr<element>(pugi::xml_node &)> wrap_factory(
    std::function<T(pugi::xml_node &)> make_object)
{
    return [make_object](pugi::xml_node &node) { return std::make_unique<T>(make_object(node)); };
}

block_factory factory = { //
    {"text:p", wrap_factory<paragraph>(make_paragraph)},
    {"text:h", wrap_factory<heading>(make_heading)},
    /*{"text:list", wrap_factory<list>(make_list)},*/
    /*{"text:list-item", wrap_factory<list_item>(make_list_item)}*/};
}

odt_file::odt_file(const std::string &filename) :
    m_zip(filename)
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
        for (auto &child : node.children())
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

        if (auto it = m_factory.find(tag); it != m_factory.end())
        {
            m_blocks.push(it->second(node));

            for (auto child : node.children())
            {
                if (child.type() == pugi::node_pcdata)
                {
                    m_blocks.top()->add_child(std::make_unique<span>(std::string(child.value())));
                }
                else
                {
                    // Recursion:
                    handle_node(child);
                }
            }

            // This level is now complete:
            auto completed_element = std::move(m_blocks.top());
            m_blocks.pop();
            if (!m_blocks.empty())
            {
                m_blocks.top()->add_child(std::move(completed_element));
            }
            else
            {
                m_doc.add_child(std::move(completed_element));
            }
        }
        else
        {
            std::cout << "Tag " << tag << " is not handled\n";
        }
    }
    block_factory m_factory;
    std::stack<std::unique_ptr<element>> m_blocks;
    text_doc m_doc;
};

text_doc odt_file::parse_text_doc()
{
    if (!m_zip.open(libzippp::ZipArchive::ReadOnly))
        throw std::runtime_error("Could not open archive");

    auto content = m_zip.getEntry("content.xml");

    if (content.isNull())
        throw std::runtime_error("Could not open content.xml");

    auto content_xml = content.readAsText();

    pugi::xml_document xml;
    if (!xml.load_string(content_xml.c_str()))
        throw std::runtime_error("Failed to parse the content.xml");

    auto doc_node = xml.child("office:document-content").child("office:body").child("office:text");

    doc_node.children();

    parser p(odt::factory);
    p.traverse(doc_node);

    return p.get();
}
}