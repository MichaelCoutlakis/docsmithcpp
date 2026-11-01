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

#include <filesystem>
#include <string>

#include "docsmithcpp/text_doc.h"

namespace docsmith::html
{

/// An HTML5 page generated from a document.
///
/// Style names on nodes become CSS classes, and the definitions in
/// doc.styles() become rules in an embedded style sheet. A list is written as
/// <ol> or <ul> according to its style in doc.list_styles(). A paragraph styled
/// builtin::preformatted (or a style based on it) is <pre>, and a span styled
/// builtin::source_text is <code>. unknown
/// (pass-through) nodes hold another format's markup and are left out.
///
///     html::file page(doc);
///     page.css = "body { max-width: 40em; margin: auto; }";
///     page.save("out.html");
class file
{
public:
    file() = default;
    explicit file(text_doc doc) :
        doc(std::move(doc))
    {
    }

    /// The page as save() would write it, image srcs included. No files are
    /// written.
    std::string to_string() const;

    /// Writes the page, and the images it needs under resource_dir: document
    /// resources as resource_dir / name, and image files from disk copied to
    /// resource_dir / Pictures/ (unless already inside resource_dir). Pages
    /// saved with the same resource_dir share these files, so give pages in
    /// one folder their own resource_dir if their images may differ.
    void save(const std::filesystem::path &p) const;

    text_doc doc;
    /// The <title>; defaults to doc.meta.title, then the text of
    /// doc.find_title(), then of the first heading.
    std::string title;
    std::string css;   //!< Extra CSS, embedded after the generated rules
    /// Folder for the page's images, relative to the page. Empty puts them
    /// next to it, e.g. an image from an .odt lands in Pictures/ there.
    std::filesystem::path resource_dir;
};

} // namespace docsmith::html
