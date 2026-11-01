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

// Private: a WordprocessingML package into the document model.

#include "docsmithcpp/text_doc.h"
#include "docx/state.h"

namespace docsmith::docx
{

/// Reads st.pkg's main document (st.main_part), header, footer, styles,
/// numbering and metadata into doc. Images the document shows are moved from
/// the package into doc.resources(). What save() needs to write back what the
/// model doesn't hold (direct formatting, drawings) is recorded in st.
void read_document(state &st, text_doc &doc);

} // namespace docsmith::docx
