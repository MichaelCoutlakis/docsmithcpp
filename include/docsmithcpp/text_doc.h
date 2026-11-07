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

// The document root, plus the umbrella include for the whole model and its
// traversal helpers.

#include <filesystem>
#include <variant>
#include <vector>

#include "docsmithcpp/list.h"
#include "docsmithcpp/metadata.h"
#include "docsmithcpp/nodes.h"
#include "docsmithcpp/page.h"
#include "docsmithcpp/resource.h"
#include "docsmithcpp/style.h"
#include "docsmithcpp/table.h"
#include "docsmithcpp/visit.h"

namespace docsmith
{

/// A template's placeholder, for text_doc::replace() and insert_*(): the first
/// paragraph or heading that holds this bookmark, has this style, or whose text
/// is this (ignoring white space around it). A string is text:
///
///     doc.replace("{{summary}}", ...);        // the paragraph that reads {{summary}}
///     doc.replace(style_name("Summary"), ...); // the first one with that style
struct placeholder
{
    placeholder(bookmark b) :
        v(std::move(b))
    {
    }
    placeholder(style_name sn) :
        v(std::move(sn))
    {
    }
    placeholder(text t) :
        v(std::move(t))
    {
    }
    placeholder(const char *s) :
        v(text{s})
    {
    }
    placeholder(std::string s) :
        v(text{std::move(s)})
    {
    }

    std::variant<bookmark, style_name, text> v;
};

class text_doc
{
public:
    text_doc() = default;

    template <typename... Args>
        requires content_args<text_doc, block_node, Args...>
    text_doc(Args &&...args) :
        body{to_block(std::forward<Args>(args))...}
    {
    }

    text_doc &add(block_node b)
    {
        body.push_back(std::move(b));
        return *this;
    }
    template <block_content T>
    text_doc &add(T &&content)
    {
        body.push_back(to_block(std::forward<T>(content)));
        return *this;
    }

    /// Appends other's body, and adds its styles, list styles and resources.
    /// A name defined in both keeps one copy if the definitions are equal, and
    /// throws std::invalid_argument (leaving this document unchanged) if not.
    /// other.meta is not copied; a title paragraph in other's body is appended
    /// like any other paragraph.
    text_doc &append(const text_doc &other);

    /// Filling in a template: puts content's body in place of, after or
    /// before the placeholder, and adds content's styles, list styles and
    /// resources as append() does. The placeholder is looked for depth first
    /// in the body (lists and table cells included), then the page header and
    /// footer. Returns false, leaving the document unchanged, if nothing
    /// matches.
    ///
    ///     doc.replace(text{"{{summary}}"}, {heading(1, "Summary"), results});
    ///     doc.insert_after(bookmark{"log"}, paragraph(log));
    bool replace(const placeholder &p, const text_doc &content);
    bool insert_after(const placeholder &p, const text_doc &content);
    bool insert_before(const placeholder &p, const text_doc &content);

    /// Compares everything, registries included. To compare content only,
    /// compare the bodies: a.body == b.body.
    bool operator==(const text_doc &) const = default;

    style_registry &styles() { return m_styles; }
    const style_registry &styles() const { return m_styles; }
    list_style_registry &list_styles() { return m_list_styles; }
    const list_style_registry &list_styles() const { return m_list_styles; }
    resource_registry &resources() { return m_resources; }
    const resource_registry &resources() const { return m_resources; }

    /// Adds the style definition, if none of that name is defined yet, and
    /// returns its name, for set_style(). Like std::map::insert, an existing
    /// definition stays as it is.
    style_name add_style(const style &s);
    style_name add_style(const list_style &ls);
    /// Adds the style definition, replacing any of that name, and returns its
    /// name. Like std::map::insert_or_assign.
    style_name add_or_overwrite_style(const style &s);
    style_name add_or_overwrite_style(const list_style &ls);

    /// The definition of a style, to edit: the document's (loaded from a
    /// template, say), else a new empty one, of the builtin's family for a
    /// builtin. Meant for the builtin defaults:
    ///
    ///     doc.builtin_style(builtin::default_paragraph)
    ///         .set(text_props(font_name("Liberation Serif"), font_size(11)));
    ///
    /// A new one starts empty, so the look a writer gives an undefined builtin
    /// (ODT's Title or Heading 1, say) isn't in it. builtin::default_list is a
    /// list style: use list_styles() or add_or_overwrite_style() for it. The
    /// reference stays valid until the style is removed from styles().
    style &builtin_style(const style_name &name);

    /// Adds data as the resource name (a relative path, e.g. "Pictures/a.png")
    /// and returns an image that refers to it.
    image add_image(std::string name, std::string data, std::string media_type);

    /// The first paragraph with the builtin::title style, or nullptr.
    paragraph *find_title();
    const paragraph *find_title() const;

    /// Sets the title paragraph's text, adding one at the start of body if
    /// there is none. meta.title is separate and left as is.
    text_doc &set_title(std::string text);

    /// Writes each resource an image in body refers to as dir / name. Pages or
    /// documents written to the same dir share these files, and a resource
    /// with the same name overwrites the other's.
    void write_resources(const std::filesystem::path &dir) const;

    metadata meta;
    std::vector<block_node> body;
    page_style page; //!< The header and footer on the body's pages

private:
    enum class fill_mode
    {
        replace,
        after,
        before
    };
    bool fill(const placeholder &p, const text_doc &content, fill_mode mode);
    /// Adds other's registries, or throws (changing nothing) on a clash.
    void merge_registries(const text_doc &other);

    style_registry m_styles;
    list_style_registry m_list_styles;
    resource_registry m_resources;
};

} // namespace docsmith
