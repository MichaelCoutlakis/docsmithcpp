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

#include <vector>

#include "docsmithcpp/nodes.h"

namespace docsmith
{

/// How the body's pages look: what each page repeats above and below the body.
/// Empty means none. Formats with pages put it on every page (ODT: the master
/// page the body uses); HTML writes it once, as <header> and <footer>.
struct page_style
{
    std::vector<block_node> header;
    std::vector<block_node> footer;

    /// Replaces the header with content, e.g.
    /// set_header(paragraph(field{field_type::title})).
    template <block_content... Args>
    page_style &set_header(Args &&...content)
    {
        header = {to_block(std::forward<Args>(content))...};
        return *this;
    }
    template <block_content... Args>
    page_style &set_footer(Args &&...content)
    {
        footer = {to_block(std::forward<Args>(content))...};
        return *this;
    }

    bool operator==(const page_style &) const = default;
};

} // namespace docsmith
