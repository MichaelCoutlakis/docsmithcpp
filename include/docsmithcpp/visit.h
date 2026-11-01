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

// Traversal and search over the document model. Replaces the old
// element_base::find_all / get_elem_of and the styled_base interface.

#include <type_traits>
#include <variant>
#include <vector>

#include "docsmithcpp/nodes.h"

namespace docsmith
{

/// The "overloaded" idiom: assemble a visitor from a set of lambdas. Use with
/// std::visit on inline_node::v / block_node::v.
template <typename... Fs>
struct overloaded : Fs...
{
    using Fs::operator()...;
};
template <typename... Fs>
overloaded(Fs...) -> overloaded<Fs...>;

namespace detail
{
/// Applies a callable to every node in the tree, depth first. Works for both
/// const and non-const nodes: the node's constness flows through to the
/// callable, so it can read or mutate.
template <typename F>
struct deep_visitor
{
    F &f;

    template <typename N>
    void node(N &n)
    {
        f(n);
        using T = std::decay_t<N>;
        auto recurse = [this](auto &child_variant)
        { std::visit([this](auto &alt) { node(alt); }, child_variant); };

        if constexpr(std::is_same_v<T, frame>)
            node(n.img);
        else if constexpr(std::is_same_v<T, span> || std::is_same_v<T, hyperlink> ||
            std::is_same_v<T, paragraph> || std::is_same_v<T, heading>)
            for(auto &c : n.children()) // accessor hides any boxing
                recurse(c.v);
        else if constexpr(std::is_same_v<T, list>)
            for(auto &it : n.items())
                node(it);
        else if constexpr(std::is_same_v<T, list_item> || std::is_same_v<T, table_cell>)
            for(auto &b : n.children())
                recurse(b.v);
        else if constexpr(std::is_same_v<T, table>)
            for(auto &c : n.cells())
                node(c);
        // text, bookmark, image, page_break, unknown have no children.
    }
};
} // namespace detail

/// Apply f to every node in the document (depth first): the body, then the
/// page header and footer.
template <typename Doc, typename F>
void walk(Doc &doc, F f)
{
    detail::deep_visitor<F> dv{f};
    for(auto *blocks : {&doc.body, &doc.page.header, &doc.page.footer})
        for(auto &b : *blocks)
            std::visit([&dv](auto &alt) { dv.node(alt); }, b.v);
}

/// Collect pointers to every node of type T in the document.
template <typename T, typename Doc>
auto collect(Doc &doc)
{
    using ptr = std::conditional_t<std::is_const_v<Doc>, const T *, T *>;
    std::vector<ptr> out;
    walk(doc,
        [&out](auto &n)
        {
            if constexpr(std::is_same_v<std::decay_t<decltype(n)>, T>)
                out.push_back(&n);
        });
    return out;
}

/// Collect pointers to every node of type T satisfying pred.
template <typename T, typename Doc, typename Pred>
auto find_all(Doc &doc, Pred pred)
{
    using ptr = std::conditional_t<std::is_const_v<Doc>, const T *, T *>;
    std::vector<ptr> out;
    walk(doc,
        [&](auto &n)
        {
            if constexpr(std::is_same_v<std::decay_t<decltype(n)>, T>)
                if(pred(n))
                    out.push_back(&n);
        });
    return out;
}

/// Clear the style name on every node that carries one, and on table rows and
/// columns. Handy for comparing a parsed document against a code-built one
/// without minding style names.
/// Templated on the document type so this header needs only nodes.h (text_doc
/// is defined in text_doc.h, which includes this header).
template <typename Doc>
void strip_styles(Doc &doc)
{
    walk(doc,
        [](auto &n)
        {
            if constexpr(requires { n.style; })
                n.style = style_name{};
            if constexpr(std::is_same_v<std::decay_t<decltype(n)>, table>)
            {
                for(size_t r = 0; r != n.num_rows(); ++r)
                    n.set_row_style(r, {});
                for(size_t c = 0; c != n.num_cols(); ++c)
                    n.set_column_style(c, {});
            }
        });
}

} // namespace docsmith
