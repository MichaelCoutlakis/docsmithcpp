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

#include "docsmithcpp/box.h"
#include "docsmithcpp/text_doc.h"

namespace docsmith::docx
{

/// A .docx file (Office Open XML WordprocessingML): the document model plus
/// the rest of the package. Styles are written to styles.xml, list styles to
/// numbering.xml, doc.meta to docProps/core.xml, and the page header and
/// footer to header and footer parts. Parts the model doesn't represent are
/// kept from load() and written back by save(), so load, edit, save doesn't
/// lose them; so is formatting set directly on runs and paragraphs, which
/// the model holds as styles T1, P1, ...
///
///     auto f = docx::file::load("in.docx");
///     f.doc.add(paragraph("appended"));
///     f.save("out.docx");
///
///     docx::file(text_doc{heading(1, "New")}).save("new.docx");
class file
{
public:
    file();
    explicit file(text_doc doc);

    /// Images the document shows are moved from the package into
    /// doc.resources(), named by their part ("word/media/image1.png").
    static file load(const std::filesystem::path &p);

    /// Images whose uri names one of doc.resources() are embedded from it;
    /// other image uris are read from disk. Both go under word/media/.
    void save(const std::filesystem::path &p) const;

    text_doc doc;

private:
    struct impl;
    box<impl> m_impl; // pimpl: copyable, and hides the archive / XML libraries
};

} // namespace docsmith::docx
