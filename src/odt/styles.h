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

// Private: style definitions to and from ODF XML. read_*() is the inverse of
// write_xml() for the properties the model holds; the rest are left out.

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <pugixml.hpp>

#include "docsmithcpp/text_doc.h"

namespace docsmith::odf
{
class package;
}

namespace docsmith::odt
{

/// style:name, and style:display-name if it differs.
void write_name(pugi::xml_node &n, const std::string &name);
void write_xml(pugi::xml_node &n, const paragraph_style &s);
void write_xml(pugi::xml_node &n, const text_style &s);
void write_xml(pugi::xml_node &n, const table_style &s);
void write_xml(pugi::xml_node &n, const cell_style &s);
void write_xml(pugi::xml_node &n, const row_style &s);
void write_xml(pugi::xml_node &n, const frame_style &s);
void write_xml(pugi::xml_node &n, const list_style &ls);
void write_xml(pugi::xml_node &n, const bullet_level &b);
void write_xml(pugi::xml_node &n, const number_level &b);
void write_xml(pugi::xml_node &n, const text_props &props);
void write_xml(pugi::xml_node &n, const paragraph_props &props);
void write_xml(pugi::xml_node &n, const table_props &props);
void write_xml(pugi::xml_node &n, const cell_props &props);

/// The style:family of a family's styles; empty for list styles, which are
/// text:list-style elements.
const char *family_name(style_family f);

/// A length such as "0.5pt" or "1.2cm".
std::optional<length> length_from(std::string_view odf);
/// A table-column style's width: its relative width if it has one, else its
/// width in points.
std::optional<double> read_column_width(const pugi::xml_node &n);

/// A style:style the model has a family for.
using odf_style =
    std::variant<paragraph_style, text_style, table_style, cell_style, row_style, frame_style>;

/// A style:style, or nullopt if the model has no family for it.
std::optional<odf_style> read_style(const pugi::xml_node &n);
/// A text:list-style. Levels the model can't represent (e.g. images, or no
/// numbering) are left out.
list_style read_list_style(const pugi::xml_node &n);

/// The automatic paragraph styles the writer makes for a page_break: base's
/// definition (or base as parent) plus a break before. page_break_only is for
/// an empty paragraph standing in for a page break with no paragraph after it.
std::string page_break_style_name(const para_style_name &base);
inline constexpr const char *page_break_only = "PageBreakOnly";

/// Whether n is one of the page break styles above, and if so the name of the
/// style it was made from (empty for none, or for page_break_only).
std::optional<para_style_name> page_break_base(const pugi::xml_node &n);

/// Adds the definitions in pkg's styles.xml (common styles) and content.xml
/// (common and automatic styles) to doc's registries. A name already defined
/// in its family keeps its first definition.
void read_styles(const odf::package &pkg, text_doc &doc);

} // namespace docsmith::odt
