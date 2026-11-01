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

/// Text as it reads: white space is kept as is, '\t' is a tab and '\n' a line
/// break. Writers encode them as their format needs (e.g. text:s, text:tab and
/// text:line-break in ODT, <br> in HTML).
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

/// uri is resolved in this order: a resource of that name in the document's
/// resources(), then a file on disk, then left as is (e.g. a URL in HTML).
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

/// A link to a URL, used as is (e.g. "https://example.com", "mailto:a@b.org").
struct url_target
{
    std::string url;
    bool operator==(const url_target &) const = default;
};

/// A link to the bookmark of that name in the same document. Writers spell it
/// in their format's own way (e.g. "#name" in ODT and HTML).
struct bookmark_target
{
    std::string name;
    bool operator==(const bookmark_target &) const = default;
};

using link_target = std::variant<url_target, bookmark_target>;

class hyperlink : public has_children<inline_node, hyperlink, true>
{
public:
    link_target target;

    hyperlink() = default;
    explicit hyperlink(link_target t) :
        target(std::move(t))
    {
    }
    explicit hyperlink(std::string url) :
        target(url_target{std::move(url)})
    {
    }
    explicit hyperlink(const bookmark &b) :
        target(bookmark_target{b.name})
    {
    }
    /// A target (as for the one-argument constructors) followed by content.
    template <typename Target, typename... Args,
        typename = std::enable_if_t<(sizeof...(Args) >= 1)>>
    hyperlink(Target &&t, Args &&...content) :
        hyperlink(std::forward<Target>(t))
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
