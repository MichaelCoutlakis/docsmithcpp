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

// Private: serialises the document model to an ODT content.xml.

#include <string>

#include <pugixml.hpp>

#include "docsmithcpp/text_doc.h"

namespace docsmith::odf
{
class package;
}

namespace docsmith::odt
{

/// Serialise doc as an ODT content.xml. Images are resolved in turn against
/// doc.resources() (embedded into pkg under their name), entries pkg already
/// holds (referenced in place), and files on disk (copied into pkg under
/// Pictures/). If pkg holds an original content.xml (a
/// loaded file), its namespace, font and automatic-style declarations are kept.
std::string write_content(const text_doc &doc, odf::package &pkg);

void write_xml(pugi::xml_node &n, const style_name &sn);
/// Writes s in its own family, or paragraph if it has none.
void write_xml(pugi::xml_node &n, const style &s);
void write_xml(pugi::xml_node &n, const style &s, style_family family);
void write_xml(pugi::xml_node &n, const list_style &ls);
void write_xml(pugi::xml_node &n, const list_style_bullet &b);
void write_xml(pugi::xml_node &n, const list_style_num &b);
void write_xml(pugi::xml_node &n, const text_props &props);
void write_xml(pugi::xml_node &n, const paragraph_props &props);

/// Traversal is plain recursion over the node variants (std::visit): each
/// emit() appends to the parent XML node it is handed and recurses into the
/// node's children.
class writer
{
public:
    explicit writer(odf::package &pkg);
    std::string write(const text_doc &doc);

private:
    void carry_over();
    void write_doc(const text_doc &doc);
    void write_block(pugi::xml_node parent, const block_node &b);
    void write_inline(pugi::xml_node parent, const inline_node &i);
    std::string resolve_image(const std::string &uri);

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

    odf::package &m_pkg;
    const text_doc *m_doc{nullptr}; //!< The document being written
    pugi::xml_document m_content; //!< content.xml; owns the node memory
    pugi::xml_node m_font_faces;  //!< office:font-face-decls
    pugi::xml_node m_styles;      //!< office:styles
    pugi::xml_node m_automatic_styles;
    pugi::xml_node m_body;        //!< office:body
};

} // namespace docsmith::odt
