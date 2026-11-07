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
#include "cell_text.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace docsmith::detail
{
namespace
{
/// Gives the unstyled paragraphs in blocks the style sn, including those in
/// lists, but not those in a nested table, whose cells have their own.
void set_unstyled(std::vector<block_node> &blocks, const para_style_name &sn)
{
    for(auto &b : blocks)
        if(auto *p = std::get_if<paragraph>(&b.v); p && p->style.empty())
            p->set_style(sn);
        else if(auto *l = std::get_if<list>(&b.v))
            for(auto &item : l->items())
                set_unstyled(item.children(), sn);
}

bool any_cell_text(const style_sheet &styles)
{
    for(const auto &[name, s] : styles.cell)
        if(!s.para.empty() || !s.text.empty())
            return true;
    return false;
}
} // namespace

std::optional<text_doc> with_cell_text(const text_doc &doc)
{
    if(!any_cell_text(doc.styles()))
        return std::nullopt;

    text_doc out = doc;
    style_sheet &styles = out.styles();
    // One style per distinct definition, named after the cell style it comes from.
    std::vector<std::pair<std::tuple<std::string, para_style_name, cell_text>, para_style_name>>
        made;
    auto style_for =
        [&](const std::string &base, const para_style_name &parent, const cell_text &ct)
    {
        const auto key = std::tuple{base, parent, ct};
        for(const auto &[k, name] : made)
            if(k == key)
                return name;
        // A loaded file has it already if it was saved before: one with the
        // same definition is used again.
        std::string name = base + "_20_Text";
        paragraph_style s(name, ct.para, ct.text);
        s.parent = parent;
        for(int i = 2; styles.para.contains(name) && *styles.para.find(name) != s; ++i)
        {
            name = base + "_20_Text_20_" + std::to_string(i);
            s = paragraph_style(name, ct.para, ct.text);
            s.parent = parent;
        }
        styles.insert(s);
        made.emplace_back(key, s.name());
        return s.name();
    };

    walk(out,
        [&](auto &n)
        {
            if constexpr(std::is_same_v<std::decay_t<decltype(n)>, table>)
                for(size_t r = 0; r != n.num_rows(); ++r)
                    for(size_t c = 0; c != n.num_cols(); ++c)
                    {
                        if(n.is_cell_covered(r, c))
                            continue;
                        const cell_text ct = effective_cell_text(n, styles, r, c);
                        if(ct.empty())
                            continue;
                        table_cell &cell = n.cell(r, c);
                        para_style_name parent =
                            effective_cell_props(n, styles, r, c).m_paragraph_style;
                        if(parent.empty())
                            parent = builtin::para;
                        const std::string &base =
                            (cell.style.empty() ? n.column_style(c) : cell.style).str();
                        set_unstyled(cell.children(), style_for(base, parent, ct));
                    }
        });
    return out;
}

} // namespace docsmith::detail
