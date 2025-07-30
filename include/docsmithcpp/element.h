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
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <iostream>

namespace docsmith
{

/// Base class for all document elements
class element
{
public:
    virtual ~element() = default;
    virtual void accept(class element_visitor &) const = 0;
    virtual std::unique_ptr<element> clone() const = 0;
    virtual bool is_equal(const element &other) const { return false; }

    virtual void add_child(std::unique_ptr<element> child)
    {
        throw std::logic_error("This element does not accept children");
    }
};

// clang-format off

// Helper to define allowed relationships between elements:
template <typename Parent, typename Child>
struct is_valid_child : std::false_type {};

template <typename Parent, typename Child>
constexpr bool is_valid_child_v = is_valid_child<Parent, Child>::value;

// Utility to help print the name during compilation when there are template compile errors
template<typename T>
struct RevealType;

// clang-format on

struct element_visitor
{
    virtual ~element_visitor() = default;

    virtual void visit(const class text &) { }
    virtual void visit(const class span &) = 0;
    virtual void visit(const class heading &) = 0;
    virtual void visit(const class paragraph &) = 0;

    virtual void visit(const class hyperlink &) { }
    virtual void visit(const class text_doc &) = 0;

    virtual void visit(const class list &) { }
    virtual void visit(const class list_item &) { }
    virtual void visit(const class list_style_num &) { }
    virtual void visit(const class list_style_bullet &) { }

    virtual void visit(const class frame &) { }
    virtual void visit(const class image &) { }

    virtual void push() { }
    virtual void pop() { }

    template <typename Unhandled>
    void visit(const Unhandled &value)
    {
    }
};


} // namespace docsmith
