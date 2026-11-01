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

// Private: style and list style definitions to and from WordprocessingML.
// read_*() is the inverse of write_*() for the properties the model holds.

#include <optional>
#include <string>

#include <pugixml.hpp>

#include "docsmithcpp/text_doc.h"

namespace docsmith::docx
{

/// w:type for a family, or nullptr for the families DOCX has no styles for.
const char *style_type(style_family f);

/// Sets the properties props holds on a w:rPr / w:pPr, in schema order.
void write_rpr(pugi::xml_node rpr, const text_props &props);
void write_ppr(pugi::xml_node ppr, const paragraph_props &props);

std::optional<text_props> read_rpr(const pugi::xml_node &rpr);
std::optional<paragraph_props> read_ppr(const pugi::xml_node &ppr);

/// Fills an empty w:style with s, in the family type (style_type()), as id.
/// link: the id of the linked style of the other family, if any.
void write_style(pugi::xml_node n, const style &s, const char *type, const std::string &id,
    const std::string &based_on, const std::string &link = {});

/// A w:style named by its w:styleId, or nullopt for numbering styles.
std::optional<style> read_style(const pugi::xml_node &n);

/// Fills an empty w:abstractNum with id and ls's levels, named ls's name.
void write_abstract_num(pugi::xml_node n, int id, const list_style &ls);

/// A w:abstractNum as a list style called name. Levels the model can't
/// represent (no numbering, other number formats, picture bullets) are left out.
list_style read_abstract_num(const pugi::xml_node &n, const style_name &name);

/// The indent of list level ilvl (0-based), in twips: left, hanging.
inline int list_indent(int ilvl) { return 720 * (ilvl + 1); }
inline constexpr int list_hanging = 360;

} // namespace docsmith::docx
