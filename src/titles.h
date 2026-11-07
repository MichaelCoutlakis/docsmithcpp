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

// Private: a title is a paragraph in the title style in every format. Writers
// write one as that paragraph, and readers read such a paragraph back as one.

#include <utility>
#include <vector>

#include "docsmithcpp/text_doc.h"
#include "lineage.h"

namespace docsmith::detail
{

/// The paragraph a title is written as: its content, in its style or
/// builtin::title.
inline paragraph as_paragraph(const title &t)
{
    paragraph p;
    p.style = t.style.empty() ? builtin::title : t.style;
    p.items() = t.items();
    return p;
}

/// Turns each paragraph in blocks, and in the lists and tables in them, whose
/// style is builtin::title or based on it into a title, unstyled if it was
/// builtin::title.
inline void read_titles(std::vector<block_node> &blocks, const style_sheet &styles)
{
    for(auto &b : blocks)
    {
        if(auto *p = std::get_if<paragraph>(&b.v))
        {
            if(!based_on(p->style, builtin::title, styles))
                continue;
            title t;
            if(p->style != builtin::title)
                t.style = p->style;
            t.items() = std::move(p->items());
            b.v = std::move(t);
        }
        else if(auto *l = std::get_if<list>(&b.v))
        {
            for(auto &it : l->items())
                read_titles(it.items(), styles);
        }
        else if(auto *tb = std::get_if<table>(&b.v))
        {
            for(auto &c : tb->cells())
                read_titles(c.items(), styles);
        }
    }
}

/// read_titles() over the body, header and footer.
inline void read_titles(text_doc &doc)
{
    for(auto *blocks : {&doc.body, &doc.page.header, &doc.page.footer})
        read_titles(*blocks, doc.styles());
}

} // namespace docsmith::detail
