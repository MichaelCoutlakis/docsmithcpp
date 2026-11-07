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

// Private: what a docx::file keeps from load() for save(), besides the model.

#include <map>
#include <string>

#include "docsmithcpp/style.h"
#include "opc/package.h"

namespace docsmith::docx
{

/// Formatting set directly on a run or paragraph (w:rPr, w:pPr), which the
/// model holds as a style made up for it (T1, P1, ...), like ODF's automatic
/// styles. While the definition is as read, the writer writes the original
/// XML back in place of the style.
template <typename Style>
struct direct_format
{
    Style def;       //!< As read: the run's or paragraph's style is its parent
    std::string xml; //!< The w:rPr or w:pPr, without what the writer sets itself
};

/// A loaded image's w:drawing (size, position, alt text), written again while
/// a frame in the same part shows that image.
struct drawing
{
    std::string part;
    std::string xml;
};

struct state
{
    opc::package pkg;      //!< Everything in the archive; empty for a new document
    std::string main_part; //!< The main document part; empty for a new document
    /// By style name: a paragraph's (P1, ...) and a run's (T1, ...).
    std::map<std::string, direct_format<paragraph_style>> direct_para;
    std::map<std::string, direct_format<text_style>> direct_text;
    std::map<std::string, drawing> drawings;     //!< By image uri
};

} // namespace docsmith::docx
