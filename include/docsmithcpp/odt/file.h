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

namespace docsmith::odt
{

/// An .odt file: the document model plus the rest of the ODF package
/// (styles.xml, meta.xml, embedded images, ...). Parts the model doesn't
/// represent are kept from load() and written back by save(), so load, edit,
/// save doesn't lose them. doc.meta is read from meta.xml and written back
/// into it, keeping the properties the model doesn't hold. Style definitions
/// are read into doc.styles() / list_styles(); unchanged ones are written back
/// as they were.
///
///     auto f = odt::file::load("in.odt");
///     f.doc.add(paragraph("appended"));
///     f.save("out.odt");
///
///     odt::file(text_doc{heading(1, "New")}).save("new.odt");
class file
{
public:
    file();
    explicit file(text_doc doc);

    /// Images the document refers to are moved from the package into
    /// doc.resources().
    static file load(const std::filesystem::path &p);

    /// Images whose uri names one of doc.resources() are embedded from it;
    /// other image uris are read from disk and embedded under Pictures/.
    void save(const std::filesystem::path &p) const;

    text_doc doc;

private:
    struct impl;
    box<impl> m_impl; // pimpl: copyable, and hides the archive / XML libraries
};

} // namespace docsmith::odt
