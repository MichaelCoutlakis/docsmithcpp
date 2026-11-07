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

// Private: writing a cell style's paragraph and text properties.

#include <optional>

#include "docsmithcpp/text_doc.h"

namespace docsmith::detail
{

/// doc with each table cell's paragraph and text properties (cell_style::para
/// and text, from its column's and its own cell style) put on the cell's
/// paragraphs that have no style: they get a paragraph style made from them,
/// based on the cell's paragraph style (cell_props::m_paragraph_style) or
/// builtin::para. No format applies them from a cell style (LibreOffice
/// ignores them there, DOCX has no cell styles), so writers write this.
/// nullopt if no cell has any.
std::optional<text_doc> with_cell_text(const text_doc &doc);

} // namespace docsmith::detail
