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

class text_doc
{
public:
    text_doc() = default;

    template <typename... Args, typename = content_ctor_t<text_doc, Args...>>
    text_doc(Args &&...args) :
        body{to_block(std::forward<Args>(args))...}
    {
    }

    text_doc &add(block_node b)
    {
        body.push_back(std::move(b));
        return *this;
    }
    template <typename T>
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

    /// Compares everything, registries included. To compare content only,
    /// compare the bodies: a.body == b.body.
    bool operator==(const text_doc &) const = default;

    style_registry &styles() { return m_styles; }
    const style_registry &styles() const { return m_styles; }
    list_style_registry &list_styles() { return m_list_styles; }
    const list_style_registry &list_styles() const { return m_list_styles; }
    resource_registry &resources() { return m_resources; }
    const resource_registry &resources() const { return m_resources; }

    /// Adds the style definition and returns its name, for set_style(). Adding
    /// an equal definition again does nothing; a different definition under the
    /// same name throws std::invalid_argument.
    style_name add_style(const style &s);
    style_name add_style(const list_style &ls);

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
    style_registry m_styles;
    list_style_registry m_list_styles;
    resource_registry m_resources;
};

} // namespace docsmith
