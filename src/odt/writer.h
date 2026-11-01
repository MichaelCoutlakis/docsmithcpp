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

// Private: serialises the document model to an ODT's content.xml and styles.xml.

#include <set>
#include <string>
#include <vector>

#include <pugixml.hpp>

#include "docsmithcpp/text_doc.h"

namespace docsmith::odf
{
class package;
}

namespace docsmith::odt
{

/// Writes doc into pkg as content.xml and styles.xml. Images are resolved in
/// turn against doc.resources() (embedded into pkg under their name), entries
/// pkg already holds (referenced in place), and files on disk (copied into pkg
/// under Pictures/). If pkg holds a loaded file's content.xml and styles.xml,
/// their namespaces, fonts and styles are kept, and the style definitions in
/// doc replace theirs by name and family where they differ.
void write_document(const text_doc &doc, odf::package &pkg);

/// Traversal is plain recursion over the node variants (std::visit): each
/// emit() appends to the parent XML node it is handed and recurses into the
/// node's children.
class writer
{
public:
    explicit writer(odf::package &pkg);
    /// Sets content.xml and styles.xml in the package.
    void write(const text_doc &doc);

private:
    void carry_over();
    void load_styles_xml();
    void write_styles(const text_doc &doc);
    void write_doc(const text_doc &doc);
    void write_page(const page_style &page);
    void write_blocks(pugi::xml_node parent, const std::vector<block_node> &blocks);
    void write_block(pugi::xml_node parent, const block_node &b);
    std::string page_break_style(const std::string &name, const style_name &base);
    void flush_page_break(pugi::xml_node parent);
    const char *block_style(const style_name &sn);
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
    void emit(pugi::xml_node parent, const field &f);
    void emit(pugi::xml_node parent, const page_break &);
    void emit(pugi::xml_node parent, const unknown &u);
    void emit(pugi::xml_node parent, const list &l);
    void emit(pugi::xml_node parent, const list_item &it);
    void emit(pugi::xml_node parent, const table &t);
    void emit(pugi::xml_node parent, const table_cell &c);

    odf::package &m_pkg;
    const text_doc *m_doc{nullptr}; //!< The document being written
    pugi::xml_document m_content; //!< content.xml; owns the node memory
    pugi::xml_document m_styles_xml;   //!< styles.xml
    pugi::xml_node m_font_faces;       //!< content.xml's office:font-face-decls
    pugi::xml_node m_styles;           //!< styles.xml's office:styles
    pugi::xml_node m_automatic_styles; //!< content.xml's office:automatic-styles
    pugi::xml_node m_body;        //!< office:body
    /// A page_break waiting for the next paragraph or heading.
    bool m_page_break_pending{false};
    std::set<std::string> m_page_break_styles; //!< Made in this write
    std::string m_block_style;                 //!< Storage for block_style()
};

} // namespace docsmith::odt
