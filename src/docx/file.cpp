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

#include "docx/state.h"
#include "docx/writer.h"

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

void file::save(const std::filesystem::path &p) const
{
    // Work on a copy so saving leaves this file unchanged and can be repeated.
    state st = m_impl->st;
    write_document(doc, st);
    st.pkg.write(p);
}

} // namespace docsmith::docx
