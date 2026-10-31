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
#include <string>

#include <libzippp/libzippp.h>
#include <pugixml.hpp>

#include "docsmithcpp/text_doc.h"

namespace docsmith::odt
{

void write_xml(pugi::xml_node &n, const style_name &sn);
void write_xml(pugi::xml_node &n, const style &s);
void write_xml(pugi::xml_node &n, const list_style &ls);
void write_xml(pugi::xml_node &n, const list_style_bullet &b);
void write_xml(pugi::xml_node &n, const list_style_num &b);
void write_xml(pugi::xml_node &n, const text_props &props);
void write_xml(pugi::xml_node &n, const paragraph_props &props);

/// Serializes a text_doc to an .odt archive. Traversal is plain recursion over
/// the node variants (std::visit) -- each emit() appends to the parent XML node
/// it is handed and recurses into the node's children.
class writer
{
public:
    static void write(const text_doc &doc, const std::string &filename);

private:
    writer();

    void write_doc(const text_doc &doc);
    void write_block(pugi::xml_node parent, const block_node &b);
    void write_inline(pugi::xml_node parent, const inline_node &i);

    void emit(pugi::xml_node parent, const paragraph &p);
    void emit(pugi::xml_node parent, const heading &h);
    void emit(pugi::xml_node parent, const span &s);
    void emit(pugi::xml_node parent, const hyperlink &h);
    void emit(pugi::xml_node parent, const text &t);
    void emit(pugi::xml_node parent, const frame &f);
    void emit(pugi::xml_node parent, const image &i);
    void emit(pugi::xml_node parent, const bookmark &b);
    void emit(pugi::xml_node parent, const unknown &u);
    void emit(pugi::xml_node parent, const list &l);
    void emit(pugi::xml_node parent, const list_item &it);
    void emit(pugi::xml_node parent, const table &t);
    void emit(pugi::xml_node parent, const table_cell &c);

    struct archive_item
    {
        std::string m_source; //!< Source on the filesystem
        std::string m_dest;   //!< Destination within the archive
        std::string m_type;   //!< Type, e.g. "image/png"
        bool operator<(const archive_item &rhs) const { return m_source < rhs.m_source; }
    };

    pugi::xml_document m_content;         //!< content.xml; owns underlying node memory
    pugi::xml_node m_body;                //!< office:body
    pugi::xml_node m_styles;              //!< office:styles
    pugi::xml_node m_automatic_styles;    //!< office:automatic-styles
    pugi::xml_document m_manifest;        //!< manifest.xml
    pugi::xml_node m_manifest_files_node; //!< manifest file list
    std::set<archive_item> m_pictures;    //!< pictures to add to the archive
};
} // namespace docsmith::odt
