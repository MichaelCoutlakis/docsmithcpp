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

#include <pugixml.hpp>

#include "docsmithcpp/text_doc.h"

namespace docsmith::odf
{
class package;
}

namespace docsmith::odt
{

void write_xml(pugi::xml_node &n, const style_name &sn);
/// Writes s in its own family, or paragraph if it has none.
void write_xml(pugi::xml_node &n, const style &s);
void write_xml(pugi::xml_node &n, const style &s, style_family family);
void write_xml(pugi::xml_node &n, const list_style &ls);
void write_xml(pugi::xml_node &n, const list_style_bullet &b);
void write_xml(pugi::xml_node &n, const list_style_num &b);
void write_xml(pugi::xml_node &n, const text_props &props);
void write_xml(pugi::xml_node &n, const paragraph_props &props);
void write_xml(pugi::xml_node &n, const graphics_props &props);

/// A style:style, or nullopt if its family has no style_family.
std::optional<style> read_style(const pugi::xml_node &n);
/// A text:list-style. Levels the model can't represent (e.g. images, or no
/// numbering) are left out.
list_style read_list_style(const pugi::xml_node &n);

/// The automatic paragraph styles the writer makes for a page_break: base's
/// definition (or base as parent) plus a break before. page_break_only is for
/// an empty paragraph standing in for a page break with no paragraph after it.
std::string page_break_style_name(const style_name &base);
inline constexpr const char *page_break_only = "PageBreakOnly";

/// Whether n is one of the page break styles above, and if so the name of the
/// style it was made from (empty for none, or for page_break_only).
std::optional<style_name> page_break_base(const pugi::xml_node &n);

/// Adds the definitions in pkg's styles.xml (common styles) and content.xml
/// (common and automatic styles) to doc's registries. A name already defined
/// keeps its first definition: ODF names are unique only per family.
void read_styles(const odf::package &pkg, text_doc &doc);

} // namespace docsmith::odt
