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
#pragma once
#include <set>
#include <stack>

#include <libzippp/libzippp.h>
#include <pugixml.hpp>

#include "docsmithcpp/text_doc.h"

namespace docsmith::odt
{
class writer : private element_visitor
{
public:
    static void write(const text_doc &doc, const std::string &filename);

private:
    void visit(const class text &) override;
    void visit(const class span &) override;

    void visit(const class heading &) override;
    void visit(const class paragraph &) override;

    void visit(const class hyperlink &) override;
    void visit(const class text_doc &) override;

    void visit(const class list &) override;
    void visit(const class list_item &) override;
    void visit(const class list_style_num &) override;
    void visit(const class list_style_bullet &) override;

    void visit(const class frame &) override;
    void visit(const class image &) override;

    void push() override;
    void pop() override;

private:
    std::string m_filename;

    struct archive_item
    {
        std::string m_source; //!< Source on the filesystem
        std::string m_dest;   //!< Destination within the archive
        std::string m_type;   //!< Type, e.g. "image/png"
        constexpr bool operator<(const archive_item &rhs) const { return m_source < rhs.m_source; }
    };

    pugi::xml_document m_content;         //!< Content for content.xml. Owns underlying node memory
    pugi::xml_document m_manifest;        //!< Content for manifest.xml
    pugi::xml_node m_manifest_files_node; //!< Node containing mainifest file list
    std::set<archive_item> m_pictures;    //!< Pictures to add to the archive

    std::stack<pugi::xml_node> m_node_stack;

    pugi::xml_node m_current;
    pugi::xml_node get_current();

    writer(std::string filename);
};
}
