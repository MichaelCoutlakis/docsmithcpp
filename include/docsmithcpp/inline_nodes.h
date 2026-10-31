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

// Inline-level (text-flow) content and the inline_node variant.

#include <string>
#include <variant>

#include "docsmithcpp/nodes_fwd.h"

namespace docsmith
{

struct text
{
    std::string content;
    bool operator==(const text &) const = default;
};

struct bookmark
{
    std::string name;
    bool operator==(const bookmark &) const = default;
};

struct image
{
    std::string uri;
    bool operator==(const image &) const = default;
};

// A draw frame; in ODT it wraps a single drawing (here, an image).
class frame : public styled<frame>
{
public:
    image img;

    frame() = default;
    explicit frame(image i) :
        img(std::move(i))
    {
    }

    bool operator==(const frame &) const = default;
};

class span : public styled<span>, public has_children<inline_node, span, true>
{
public:
    span() = default;
    template <typename... Args, typename = content_ctor_t<span, Args...>>
    span(Args &&...content)
    {
        (this->add(std::forward<Args>(content)), ...);
    }

    bool operator==(const span &) const = default;
};

class hyperlink : public has_children<inline_node, hyperlink, true>
{
public:
    std::string url;

    hyperlink() = default;
    explicit hyperlink(std::string u) :
        url(std::move(u))
    {
    }
    template <typename... Args, typename = std::enable_if_t<(sizeof...(Args) >= 1)>>
    hyperlink(std::string u, Args &&...content) :
        url(std::move(u))
    {
        (this->add(std::forward<Args>(content)), ...);
    }

    bool operator==(const hyperlink &) const = default;
};

// Pass-through for content the parser does not model: verbatim serialized XML,
// re-emitted unchanged by the writer. NOTE: only block- and inline-level foreign
// content round-trips this way; unrecognized children *inside* a table or list,
// and unknown attributes on known elements, are not yet captured.
struct unknown
{
    std::string xml;
    bool operator==(const unknown &) const = default;
};

struct inline_node
{
    std::variant<text, span, hyperlink, frame, bookmark, unknown> v;
    bool operator==(const inline_node &) const = default;
};

// Conversion helpers into inline_node (the variant is now complete).
inline inline_node to_inline(const char *s) { return inline_node{text{s}}; }
inline inline_node to_inline(const std::string &s) { return inline_node{text{s}}; }
inline inline_node to_inline(inline_node n) { return n; }
template <typename T>
inline_node to_inline(T leaf)
{
    return inline_node{std::move(leaf)};
}

} // namespace docsmith
