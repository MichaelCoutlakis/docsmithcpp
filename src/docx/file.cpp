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
#include "docsmithcpp/docx/file.h"
#include "docsmithcpp/error.h"

#include <stdexcept>

#include "docx/ooxml.h"
#include "docx/reader.h"
#include "docx/state.h"
#include "docx/writer.h"
#include "titles.h"

namespace docsmith::docx
{

struct file::impl
{
    state st;
};

file::file() :
    m_impl(impl{})
{
}

file::file(text_doc d) :
    doc(std::move(d)), m_impl(impl{})
{
}

file file::load(const std::filesystem::path &p)
{
    file f;
    auto &st = f.m_impl->st;
    st.pkg = opc::package::read(p);
    const auto *main = st.pkg.find_rel_by_type("", rel::office_document);
    const auto *part = main ? st.pkg.find(opc::package::resolve("", main->target)) : nullptr;
    // Documents and their relatives (.dotx templates, .docm) are all WordprocessingML.
    if(!part || part->content_type.find("wordprocessingml") == std::string::npos)
        throw error(error_code::not_a_document, "Not a Word document: " + p.string());
    st.main_part = opc::package::resolve("", main->target);
    read_document(st, f.doc);
    detail::read_titles(f.doc);
    return f;
}

void file::save(const std::filesystem::path &p) const
{
    // Work on a copy so saving leaves this file unchanged and can be repeated.
    state st = m_impl->st;
    write_document(doc, st);
    st.pkg.write(p);
}

} // namespace docsmith::docx
