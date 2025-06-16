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


#include "docsmithcpp/odt/ODT_File.h"
#include "docsmithcpp/Parser.h"

namespace docsmith
{
using block_factory = std::map<std::string, std::function<BlockType(pugi::xml_node& node)>>;

namespace odt
{
Paragraph MakeParagraph(pugi::xml_node& node)
{
    return Paragraph{};
}

Heading MakeHeading(pugi::xml_node& node)
{
    int level = node.attribute("text:outline-level").as_int();
    return Heading(level);
}

block_factory factory = {
    {"text:p", MakeParagraph},
    {"text:h", MakeHeading}
};
}

ODT_File::ODT_File(const std::string& filename) :
    m_zip(filename)
{
}
const char* node_types[] =
{
    "null", "document", "element", "pcdata", "cdata", "comment", "pi", "declaration"
};


struct parser
{
    parser(const block_factory& factory) :
        m_factory(factory)
    {
    }
    void traverse(pugi::xml_node& node)
    {
        for(auto& child : node.children())
        {
            handle_node(child);
        }
    }
    TextDoc get() { return std::get<TextDoc>(m_doc); }
private:
    void handle_node(pugi::xml_node& node)
    {
        auto tag = std::string(node.name());
        std::cout << "Processing tag " << tag << "\n";

        if(auto it = m_factory.find(tag); it != m_factory.end())
        {
            m_blocks.push(it->second(node));

            
            for(auto child : node.children())
            {
                if(child.type() == pugi::node_pcdata)
                {
                    BlockType block = std::string(child.value());
                    std::visit(AddItemVisitor{}, m_blocks.top(), block);
                }
                else
                {
                    // Recursion:
                    handle_node(child);
                }   
            }

            // This level complete
            BlockType completed_block = std::move(m_blocks.top());
            m_blocks.pop();
            if(!m_blocks.empty())
            {
                std::visit(AddItemVisitor{}, m_blocks.top(), completed_block);
            }
            else
            {
                //std::visit(AddItemVisitor{}, m_doc, completed_block);
                std::visit(AddItemVisitor{}, m_doc, completed_block);
                //m_doc.add(completed_block);
                std::cout << "asdf\n";
            }
        }
        else
        {
            std::cout << "Tag " << tag << " is not handled\n";
        }
    }
    block_factory m_factory;
    std::stack<BlockType> m_blocks;
    //TextDoc m_doc;X
    std::variant<TextDoc> m_doc;
};



TextDoc ODT_File::parse_text_doc()
{
    if(!m_zip.open(libzippp::ZipArchive::ReadOnly))
        throw std::runtime_error("Could not open archive");

    auto content = m_zip.getEntry("content.xml");

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
//
//
//TextDoc ODT_File::parse_text_doc()
//{
//    if(!m_zip.open(libzippp::ZipArchive::ReadOnly))
//        throw std::runtime_error("Could not open archive");
//
//    auto content = m_zip.getEntry("content.xml");
//
//    if(content.isNull())
//        throw std::runtime_error("Could not open content.xml");
//
//    auto content_xml = content.readAsText();
//
//    pugi::xml_document xml;
//    if(!xml.load_string(content_xml.c_str()))
//        throw std::runtime_error("Failed to parse the content.xml");
//
//    auto doc_node = xml.child("office:document-content").child("office:body").child("office:text");
//
//    doc_node.children();
//
//    struct XML_Traverser : public pugi::xml_tree_walker
//    {
//        virtual bool for_each(pugi::xml_node& node)
//        {
//            //if(!m_depth)
//            //    m_depth = depth();
//
//            //if(depth() < m_depth.value())
//            //    return false;
//            int depth_now = depth();
//            std::cout << "depth: " << depth_now << " ";
//
//            if(!m_depth.has_value() || depth() > m_depth.value())
//            {
//                // Push a new block:
//                std::cout << "Enter block: " << node_types[node.type()] << ": name='" << node.name() << "', value='" << node.value() << "'\n";
//                if(auto make_block = odt::factory.find(node.name()); make_block != odt::factory.end())
//                {
//                    BlockType block = make_block->second(node);
//                    m_blocks.push(block);
//                }
//            }
//            else if(depth() < m_depth.value())
//            {
//                // Pop a block
//                std::cout << "Leaving block: " << node_types[node.type()] << ": name='" << node.name() << "', value='" << node.value() << "'\n";
//                // Only pop the stack if there was a factory to push it:
//                if(auto make_block = odt::factory.find(node.name()); make_block != odt::factory.end())
//                {
//                    BlockType top_block = std::move(m_blocks.top());
//                    m_blocks.pop();
//                    std::visit(AddItemVisitor{}, m_blocks.top(), top_block);
//                }
//            }
//            else
//            {
//                // Same depth: Add to the current block
//                std::cout << "Same depth block: " << node_types[node.type()] << ": name='" << node.name() << "', value='" << node.value() << "'\n";
//                if(auto make_block = odt::factory.find(node.name()); make_block != odt::factory.end())
//                {
//                    std::visit(AddItemVisitor{}, m_blocks.top(), make_block->second(node));
//                }
//            }
//            m_depth = depth(); // New value:
//
//            for(int i = 0; i < depth(); ++i) std::cout << "  "; // indentation
//            std::cout << node_types[node.type()] << ": name='" << node.name() << "', value='" << node.value() << "'\n";
//            return true;
//        }
//    private:
//        std::optional<int> m_depth;
//        std::stack<BlockType> m_blocks;
//    };
//
//    XML_Traverser t{};
//    doc_node.traverse(t);
//    return TextDoc();
//}
}