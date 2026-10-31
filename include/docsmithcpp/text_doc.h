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

// The document root, plus the umbrella include for the whole model and its
// traversal helpers.

#include <vector>

#include "docsmithcpp/list.h"
#include "docsmithcpp/nodes.h"
#include "docsmithcpp/style.h"
#include "docsmithcpp/table.h"
#include "docsmithcpp/visit.h"

namespace docsmith
{

class text_doc
{
public:
    text_doc() = default;

    template <typename... Args, typename = content_ctor_t<text_doc, Args...>>
    text_doc(Args &&...args) :
        body{to_block(std::forward<Args>(args))...}
    {
    }

    text_doc &add(block_node b)
    {
        body.push_back(std::move(b));
        return *this;
    }
    template <typename T>
    text_doc &add(T &&content)
    {
        body.push_back(to_block(std::forward<T>(content)));
        return *this;
    }

    // Equality is about content; the style registries are metadata.
    bool operator==(const text_doc &other) const { return body == other.body; }

    style_registry &styles() { return m_styles; }
    const style_registry &styles() const { return m_styles; }
    list_style_registry &list_styles() { return m_list_styles; }
    const list_style_registry &list_styles() const { return m_list_styles; }

    std::vector<block_node> body;

private:
    style_registry m_styles;
    list_style_registry m_list_styles;
};

} // namespace docsmith
