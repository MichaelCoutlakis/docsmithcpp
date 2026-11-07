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

// ============================================================================
//  The block-level glue: the paragraph/heading text nodes and the block_node
//  variant that ties the block-level alternatives together. Including this
//  header pulls in the whole node model.
//
//  Model overview: nodes are plain value types held in std::variant, in two
//  groups matching the HTML / ODT content models -- inline_node (text-flow
//  content) and block_node (paragraph-level content). It is deliberately a
//  format-agnostic ("generic document") model: which group may go where is
//  captured by the child sequence's type, not a per-format relation table.
//  Copy and operator== are compiler generated; traversal is std::visit
//  (visit.h). Construction is via each node's constructors (scalar attributes)
//  plus a fluent .add() for children. Shared behaviour lives in the styled<>
//  (style.h) and has_children<> (nodes_fwd.h) mixins.
//
//  File map: nodes_fwd.h (fwd + mixins), inline_nodes.h (inline content),
//  list.h / table.h (list and table nodes), this header (paragraph/heading +
//  block_node), text_doc.h (the document root).
// ============================================================================

#include <variant>

#include "docsmithcpp/inline_nodes.h"
#include "docsmithcpp/list.h"
#include "docsmithcpp/nodes_fwd.h"
#include "docsmithcpp/table.h"

namespace docsmith
{

class paragraph : public styled<paragraph, style_family::para>,
                  public has_children<inline_node, paragraph, false>
{
public:
    paragraph() = default;
    template <typename... Args>
        requires content_args<paragraph, inline_node, Args...>
    paragraph(Args &&...content)
    {
        (this->add(std::forward<Args>(content)), ...);
    }

    bool operator==(const paragraph &) const = default;
};

class heading : public styled<heading, style_family::para>,
                public has_children<inline_node, heading, false>
{
public:
    int level{1};

    heading() = default;
    explicit heading(int lvl) :
        level(lvl)
    {
    }
    template <typename... Args>
        requires(sizeof...(Args) >= 1) && (inline_content<Args> && ...)
    heading(int lvl, Args &&...content) :
        level(lvl)
    {
        (this->add(std::forward<Args>(content)), ...);
    }

    bool operator==(const heading &) const = default;
};

/// Starts a new page. Writers fold it into the next paragraph or heading where
/// their format puts page breaks on paragraphs (ODT).
struct page_break
{
    bool operator==(const page_break &) const = default;
};

/// A table of contents: the body's headings down to `levels`, each linking to
/// its heading. Writers make the entries, so they follow the headings.
struct table_of_contents
{
    std::string title; //!< Shown above the entries; empty for none
    int levels{3};     //!< The deepest heading level listed

    table_of_contents() = default;
    explicit table_of_contents(std::string t, int lvls = 3) :
        title(std::move(t)), levels(lvls)
    {
    }

    bool operator==(const table_of_contents &) const = default;
};

struct block_node
{
    std::variant<paragraph, heading, list, table, frame, page_break, table_of_contents, unknown> v;
    bool operator==(const block_node &) const = default;
};

// Conversion helpers into block_node (the variant is now complete).
inline block_node to_block(const char *s) { return block_node{paragraph{s}}; }
inline block_node to_block(const std::string &s) { return block_node{paragraph{s}}; }
inline block_node to_block(block_node n) { return n; }
template <typename T>
block_node to_block(T node)
{
    return block_node{std::move(node)};
}

} // namespace docsmith
