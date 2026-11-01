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

// Private: pugixml matches names by prefix, not namespace. A file may bind any
// prefix to a namespace (or make it the default one), so before reading, every
// name is renamed to the prefix the reader expects for its namespace URI.

#include <span>
#include <string>
#include <utility>

#include <pugixml.hpp>

namespace docsmith::xml
{

/// A namespace URI and the prefix the code uses for it.
struct ns
{
    const char *prefix;
    const char *uri;
};

/// Renames every element and attribute in doc to the prefix known gives its
/// namespace, wherever and however the file declared it (default namespaces
/// and declarations on inner elements included). Namespaces not in known keep
/// their prefix, or get ns1, ns2, ... if it is taken by another namespace. The
/// declarations are removed from inner elements and all put on the root, so a
/// serialized subtree uses the root's prefixes. Prefix lists in
/// mc:Ignorable, mc:ProcessContent, mc:MustUnderstand and mc:Choice/@Requires
/// are renamed too (mc is markup compatibility, if known has it).
void normalize(pugi::xml_document &doc, std::span<const ns> known);

/// Parses data and normalizes it; false if it isn't XML. White space text is
/// kept, as it is text in a w:t.
bool load(pugi::xml_document &doc, const std::string &data, std::span<const ns> known);

/// The document as written in a package: no indentation, as indentation between
/// inline elements would be read as text.
std::string save(const pugi::xml_document &doc);

/// Serializes a subtree as is.
std::string serialize(const pugi::xml_node &n);

/// Declares the namespaces on root it doesn't declare yet.
void declare(pugi::xml_node root, std::span<const ns> namespaces);

} // namespace docsmith::xml
