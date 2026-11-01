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

// Private: plain text, and the body's headings as a table of contents lists
// them. Shared by the writers.

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "docsmithcpp/text_doc.h"

namespace docsmith::detail
{

inline void plain_text(const std::vector<inline_node> &kids, std::string &out)
{
    for(const auto &k : kids)
        std::visit(
            overloaded{
                [&](const text &t) { out += t.content; },
                [&](const span &s) { plain_text(s.children(), out); },
                [&](const hyperlink &h) { plain_text(h.children(), out); },
                [](const auto &) { },
            },
            k.v);
}

inline std::string plain_text(const std::vector<inline_node> &kids)
{
    std::string out;
    plain_text(kids, out);
    return out;
}

/// The first bookmark in kids, inside spans and links too, or nullptr.
inline const bookmark *first_bookmark(const std::vector<inline_node> &kids)
{
    for(const auto &k : kids)
    {
        const bookmark *b = std::visit(
            overloaded{
                [](const bookmark &b) { return &b; },
                [](const span &s) { return first_bookmark(s.children()); },
                [](const hyperlink &h) { return first_bookmark(h.children()); },
                [](const auto &) -> const bookmark * { return nullptr; },
            },
            k.v);
        if(b)
            return b;
    }
    return nullptr;
}

/// The body's headings, in order, wherever they are in it (not those in the
/// page header or footer).
inline std::vector<const heading *> body_headings(const text_doc &doc)
{
    std::vector<const heading *> out;
    auto f = [&out](const auto &n)
    {
        if constexpr(std::is_same_v<std::decay_t<decltype(n)>, heading>)
            out.push_back(&n);
    };
    deep_visitor<decltype(f)> dv{f};
    for(const auto &b : doc.body)
        std::visit([&dv](const auto &alt) { dv.node(alt); }, b.v);
    return out;
}

inline bool has_toc(const text_doc &doc)
{
    for(const auto &b : doc.body)
        if(std::holds_alternative<table_of_contents>(b.v))
            return true;
    return false;
}

} // namespace docsmith::detail
