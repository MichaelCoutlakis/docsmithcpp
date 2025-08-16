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
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "docsmithcpp/element.h"
#include "docsmithcpp/forward_decl.h"
#include "docsmithcpp/list.h"
#include "docsmithcpp/named_registry.h"
#include "docsmithcpp/nodes.h"
#include "docsmithcpp/style.h"
#include "docsmithcpp/text.h"

namespace docsmith
{

template <typename Derived, elem_t TagType>
class elem_tagged_nodes_styled : public element_base<Derived>,
                                 public nodes<Derived>,
                                 public styled<Derived>,
                                 public elem_tagged<Derived, TagType>
{
    using nodes<Derived>::nodes;
};

class span : public elem_tagged_nodes_styled<span, elem_t::spn>
{
public:
    using elem_tagged_nodes_styled::elem_tagged_nodes_styled;

    bool operator==(const span &rhs) const
    {
        return m_style_name == rhs.m_style_name && compare_equality(m_children, rhs.m_children);
    }

private:
    style_name m_style_name;
};

// clang-format off
template <> struct is_valid_child<span, span> : std::true_type {};
template <> struct is_valid_child<span, text> : std::true_type {};
// clang-format on

class hyperlink : public element_base<hyperlink>,
                  public nodes<hyperlink>,
                  public elem_tagged<paragraph, elem_t::href>
{
public:
    template <typename... Args>
    hyperlink(std::string url, Args &&...args) :
        nodes<hyperlink>(std::forward<Args>(args)...), m_url(url)
    {
    }

    std::string get_url() const { return m_url; }

    bool operator==(const hyperlink &rhs) const
    {
        return m_url == rhs.m_url && compare_equality(m_children, rhs.m_children);
    }

private:
    std::string m_url;
};

// clang-format off
template <> struct is_valid_child<hyperlink, span> : std::true_type {};
template <> struct is_valid_child<hyperlink, text> : std::true_type {};

// clang-format on

class image : public element_base<image>, public elem_tagged<image, elem_t::img>
{
public:
    explicit image(std::string uri) :
        m_uri(uri)
    {
    }

    std::string get_uri() const { return m_uri; }

    bool operator==(const image &rhs) const
    {
        bool b = m_uri == rhs.m_uri;
        return b;
    }

private:
    std::string m_uri;
};

class frame : public elem_tagged_nodes_styled<frame, elem_t::fr>
{
public:
    using elem_tagged_nodes_styled::elem_tagged_nodes_styled;

    bool operator==(const frame &other) const
    {
        return get_style() == other.get_style() && compare_equality(m_children, other.m_children);
    }
};

// clang-format off
template <> struct is_valid_child<frame, image> : std::true_type {};

// clang-format on

class paragraph : public elem_tagged_nodes_styled<paragraph, elem_t::p>
{
public:
    using elem_tagged_nodes_styled::elem_tagged_nodes_styled;

    bool operator==(const paragraph &other) const
    {
        return get_style() == other.get_style() && compare_equality(m_children, other.m_children);
    }

private:
};

// clang-format off
template <> struct is_valid_child<paragraph, span> : std::true_type {};
template <> struct is_valid_child<paragraph, text> : std::true_type {};
template <> struct is_valid_child<paragraph, hyperlink> : std::true_type {};
template <> struct is_valid_child<paragraph, frame> : std::true_type {};
// clang-format on

class heading : public element_base<heading>,
                public nodes<heading>,
                public styled<heading>,
                public elem_tagged<heading, elem_t::h>
{
public:
    template <typename... Args>
    heading(int level, Args &&...args) :
        nodes<heading>(std::forward<Args>(args)...), m_level(level)
    {
    }

    int level() const { return m_level; }

    bool operator==(const heading &other) const
    {
        return m_level == other.m_level && compare_equality(this->m_children, other.m_children);
    }

private:
    int m_level{1};
};

// clang-format off
template <> struct is_valid_child<heading, paragraph> : std::true_type {};
template <> struct is_valid_child<heading, span> : std::true_type {};
template <> struct is_valid_child<heading, text> : std::true_type {};
// clang-format on

class text_doc : public element_base<text_doc>,
                 public nodes<text_doc>,
                 public elem_tagged<paragraph, elem_t::doc>
{
public:
    text_doc() = default;

    template <typename... Args>
    text_doc(Args &&...args) :
        nodes<text_doc>(std::forward<Args>(args)...)
    {
    }

    bool operator==(const text_doc &other) const
    {
        return compare_equality(m_children, other.m_children);
    }

    style_registry &styles() { return m_styles; }
    const style_registry &styles() const { return m_styles; }
    list_style_registry &list_styles() { return m_list_styles; }
    const list_style_registry &list_styles() const { return m_list_styles; }

private:
    style_registry m_styles;
    list_style_registry m_list_styles;
};

// clang-format off
template <> struct is_valid_child<text_doc, heading> : std::true_type {};
template <> struct is_valid_child<text_doc, paragraph> : std::true_type {};
template <> struct is_valid_child<text_doc, list> : std::true_type {};
template <> struct is_valid_child<text_doc, frame> : std::true_type {};
// clang-format on

} // namespace docsmith
