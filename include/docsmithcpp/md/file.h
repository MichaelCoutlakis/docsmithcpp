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
#include <string_view>

#include "docsmithcpp/text_doc.h"

namespace docsmith::md
{

/// A Markdown (GitHub Flavored Markdown) file generated from, or read into, a
/// document.
///
/// Styles have no Markdown form. A span whose style is bold is **strong**, one
/// that is italic is *emphasis*, and one based on builtin::source_text is a
/// `code span`. A paragraph based on builtin::preformatted is a fenced code
/// block, and one based on builtin::quotations a block quote. Other styles are left
/// out, keeping the content. Lists are ordered or not as their list style
/// says, tables are GFM tables, bookmarks are <a id> anchors, and a
/// page_break is a thematic break. unknown (pass-through) nodes are left out.
///
///     md::file(doc).save("summary.md");
///
/// Reading needs the library built with DOCSMITHCPP_MARKDOWN:
///
///     auto f = md::file::load("in.md");
///     odt::file(f.doc).save("out.odt");
class file
{
public:
    file() = default;
    explicit file(text_doc doc) :
        doc(std::move(doc))
    {
    }

    /// The Markdown as save() would write it, image paths included. No files
    /// are written.
    [[nodiscard]] std::string to_string() const;

    /// Writes the file, and the images it needs under resource_dir, as
    /// html::file::save() does.
    void save(const std::filesystem::path &p) const;

#ifdef DOCSMITHCPP_MARKDOWN
    /// Parses the file (see parse()). Images given by a relative path to a file
    /// that exists are read into doc.resources() under that path, so the
    /// document can be saved elsewhere without them.
    [[nodiscard]] static file load(const std::filesystem::path &p);
#endif

    text_doc doc;
    /// Folder for the images, relative to the file. Empty puts them next to it.
    std::filesystem::path resource_dir;
};

/// Saves doc as Markdown, as md::file(doc).save(p) does, without copying doc.
void save(const text_doc &doc, const std::filesystem::path &p);

#ifdef DOCSMITHCPP_MARKDOWN
/// Parses CommonMark with the GFM tables, strikethrough and autolink
/// extensions. Emphasis and strong are spans with the styles
/// builtin::emphasis and builtin::strong (defined in the result's styles()),
/// code spans have builtin::source_text, code blocks are builtin::preformatted
/// paragraphs, and block quotes builtin::quotations paragraphs. Lists get the
/// list styles builtin::numbered_list or builtin::bullet_list. Raw HTML is dropped, apart from <a
/// id> anchors (bookmarks) and <br> in table cells. Strikethrough keeps its text; the model has no
/// strikethrough. Image uris are left as written.
///
/// The result can be added to another document with text_doc::append().
text_doc parse(std::string_view markdown);
#endif

} // namespace docsmith::md
