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

// Private: WordprocessingML names shared by the DOCX reader and writer.

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

#include <pugixml.hpp>

#include "xml/namespaces.h"

namespace docsmith::docx
{

/// The prefixes the code uses, whatever a file declares (see xml::normalize).
inline constexpr xml::ns namespaces[] = {
    {"w", "http://schemas.openxmlformats.org/wordprocessingml/2006/main"},
    {"r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships"},
    {"wp", "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"},
    {"a", "http://schemas.openxmlformats.org/drawingml/2006/main"},
    {"pic", "http://schemas.openxmlformats.org/drawingml/2006/picture"},
    {"mc", "http://schemas.openxmlformats.org/markup-compatibility/2006"},
    {"w14", "http://schemas.microsoft.com/office/word/2010/wordml"},
    {"w15", "http://schemas.microsoft.com/office/word/2012/wordml"},
    {"wp14", "http://schemas.microsoft.com/office/word/2010/wordprocessingDrawing"},
    {"wps", "http://schemas.microsoft.com/office/word/2010/wordprocessingShape"},
    {"wpg", "http://schemas.microsoft.com/office/word/2010/wordprocessingGroup"},
    {"m", "http://schemas.openxmlformats.org/officeDocument/2006/math"},
    {"v", "urn:schemas-microsoft-com:vml"},
    {"o", "urn:schemas-microsoft-com:office:office"},
    {"w10", "urn:schemas-microsoft-com:office:word"},
    {"cp", "http://schemas.openxmlformats.org/package/2006/metadata/core-properties"},
    {"dc", "http://purl.org/dc/elements/1.1/"},
    {"dcterms", "http://purl.org/dc/terms/"},
    {"dcmitype", "http://purl.org/dc/dcmitype/"},
    {"xsi", "http://www.w3.org/2001/XMLSchema-instance"},
    {"ep", "http://schemas.openxmlformats.org/officeDocument/2006/extended-properties"},
    {"cup", "http://schemas.openxmlformats.org/officeDocument/2006/custom-properties"},
    {"vt", "http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes"},
};

/// What the writer declares on the root of a document, header or footer part.
inline constexpr std::span<const xml::ns> body_namespaces{namespaces, 6};

inline bool load(pugi::xml_document &doc, const std::string &data)
{ return xml::load(doc, data, namespaces); }

inline const char *uri_of(std::string_view prefix)
{
    for(const auto &n : namespaces)
        if(prefix == n.prefix)
            return n.uri;
    return "";
}

/// Declares these of the namespaces above on root, unless it does already.
inline void declare(pugi::xml_node root, std::initializer_list<const char *> prefixes)
{
    for(const char *p : prefixes)
        xml::declare(root,
            std::span<const xml::ns>(
                &*std::find_if(std::begin(namespaces),
                    std::end(namespaces),
                    [p](const xml::ns &n) { return std::string_view(n.prefix) == p; }),
                1));
}

namespace rel
{
inline constexpr const char *office_document =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument";
inline constexpr const char *core_properties =
    "http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties";
inline constexpr const char *extended_properties =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties";
inline constexpr const char *custom_properties =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/custom-properties";
inline constexpr const char *styles =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles";
inline constexpr const char *numbering =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering";
inline constexpr const char *settings =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings";
inline constexpr const char *header =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/header";
inline constexpr const char *footer =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer";
inline constexpr const char *image =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image";
inline constexpr const char *hyperlink =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink";
} // namespace rel

namespace content_type
{
inline constexpr const char *document =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml";
inline constexpr const char *styles =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml";
inline constexpr const char *numbering =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml";
inline constexpr const char *settings =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml";
inline constexpr const char *header =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml";
inline constexpr const char *footer =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml";
inline constexpr const char *core = "application/vnd.openxmlformats-package.core-properties+xml";
inline constexpr const char *app =
    "application/vnd.openxmlformats-officedocument.extended-properties+xml";
inline constexpr const char *custom =
    "application/vnd.openxmlformats-officedocument.custom-properties+xml";
} // namespace content_type

/// The abstractNum the writer gives a list with no list style. It reads back
/// as an unstyled list.
inline constexpr const char *default_list_name = "DocSmith Default List";

/// Hidden bookmarks the writer adds to headings for the table of contents to
/// link to. The reader drops them, as it drops Word's own _Toc bookmarks.
inline constexpr std::string_view toc_bookmark_prefix = "_Toc";

/// The level of a heading style by Word's id (Heading1-Heading9) or name
/// (heading 1, any case), else 0.
inline int heading_level(std::string_view id, std::string_view name)
{
    auto digit = [](std::string_view rest)
    { return rest.size() == 1 && rest[0] >= '1' && rest[0] <= '9' ? rest[0] - '0' : 0; };
    if(id.starts_with("Heading"))
        if(int n = digit(id.substr(7)))
            return n;
    if(name.size() == 9)
    {
        std::string lower(name.substr(0, 8));
        for(char &c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if(lower == "heading ")
            return digit(name.substr(8));
    }
    return 0;
}

/// A style's display name: its name with ODF's escapes (_20_ for a space)
/// decoded.
inline std::string display_name(const std::string &name)
{
    auto hex = [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; };
    std::string out;
    for(std::size_t i = 0; i < name.size(); ++i)
    {
        if(name[i] == '_' && i + 3 < name.size() && hex(name[i + 1]) && hex(name[i + 2]) &&
            name[i + 3] == '_')
        {
            out += static_cast<char>(std::stoi(name.substr(i + 1, 2), nullptr, 16));
            i += 3;
        }
        else
            out += name[i];
    }
    return out;
}

/// The w:name Word gives its own styles, by id, else the id's display name.
inline std::string word_style_name(const std::string &id)
{
    if(heading_level(id, {}))
        return "heading " + id.substr(7);
    if(id == "TableNormal")
        return "Normal Table";
    return display_name(id);
}

/// The w:val attribute of parent's child of that name ("" if there's none).
inline std::string val(const pugi::xml_node &parent, const char *child)
{ return parent.child(child).attribute("w:val").as_string(); }

/// Whether an on/off property (w:b, w:i, ...) is on. No w:val means on.
inline bool on(const pugi::xml_node &n)
{
    const std::string_view v = n.attribute("w:val").as_string("true");
    return v != "0" && v != "false" && v != "off";
}

/// The child named name of parent, added where order puts it if there's none:
/// WordprocessingML properties are sequences, and Word rejects them out of
/// order. Names not in order go last.
template <std::size_t N>
pugi::xml_node ordered_child(
    pugi::xml_node parent, const char *name, const std::array<const char *, N> &order)
{
    if(auto c = parent.child(name))
        return c;
    auto rank = [&](std::string_view n)
    {
        for(std::size_t i = 0; i != N; ++i)
            if(n == order[i])
                return i;
        return N;
    };
    const std::size_t r = rank(name);
    for(auto c : parent.children())
        if(c.type() == pugi::node_element && rank(c.name()) > r)
            return parent.insert_child_before(name, c);
    return parent.append_child(name);
}

// The orders of the property sequences the writer adds to.
inline constexpr std::array<const char *, 35> ppr_order{"w:pStyle",
    "w:keepNext",
    "w:keepLines",
    "w:pageBreakBefore",
    "w:framePr",
    "w:widowControl",
    "w:numPr",
    "w:suppressLineNumbers",
    "w:pBdr",
    "w:shd",
    "w:tabs",
    "w:suppressAutoHyphens",
    "w:kinsoku",
    "w:wordWrap",
    "w:overflowPunct",
    "w:topLinePunct",
    "w:autoSpaceDE",
    "w:autoSpaceDN",
    "w:bidi",
    "w:adjustRightInd",
    "w:snapToGrid",
    "w:spacing",
    "w:ind",
    "w:contextualSpacing",
    "w:mirrorIndents",
    "w:suppressOverlap",
    "w:jc",
    "w:textDirection",
    "w:textAlignment",
    "w:textboxTightWrap",
    "w:outlineLvl",
    "w:divId",
    "w:cnfStyle",
    "w:rPr",
    "w:sectPr"};
inline constexpr std::array<const char *, 27> rpr_order{"w:rStyle",
    "w:rFonts",
    "w:b",
    "w:bCs",
    "w:i",
    "w:iCs",
    "w:caps",
    "w:smallCaps",
    "w:strike",
    "w:dstrike",
    "w:outline",
    "w:shadow",
    "w:emboss",
    "w:imprint",
    "w:noProof",
    "w:snapToGrid",
    "w:vanish",
    "w:webHidden",
    "w:color",
    "w:spacing",
    "w:w",
    "w:kern",
    "w:position",
    "w:sz",
    "w:szCs",
    "w:highlight",
    "w:u"};
inline constexpr std::array<const char *, 19> style_order{"w:name",
    "w:aliases",
    "w:basedOn",
    "w:next",
    "w:link",
    "w:autoRedefine",
    "w:hidden",
    "w:uiPriority",
    "w:semiHidden",
    "w:unhideWhenUsed",
    "w:qFormat",
    "w:locked",
    "w:personal",
    "w:personalCompose",
    "w:personalReply",
    "w:rsid",
    "w:pPr",
    "w:rPr",
    "w:tblPr"};
inline constexpr std::array<const char *, 5> sect_order{
    "w:headerReference", "w:footerReference", "w:footnotePr", "w:endnotePr", "w:type"};

} // namespace docsmith::docx
