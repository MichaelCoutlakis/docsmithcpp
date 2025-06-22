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
#include <type_traits>

namespace docsmith
{

/// Base class for all document elements
class element
{
public:
    virtual ~element() = default;
    virtual void accept(class element_visitor &) const = 0;
    virtual std::unique_ptr<element> clone() const = 0;
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

    virtual void visit(const class span &) = 0;
    virtual void visit(const class heading &) = 0;
    virtual void visit(const class paragraph &) = 0;
    virtual void visit(const class text_doc &) = 0;

    template <typename UnhandledType>
    void visit(const UnhandledType &value)
    {
        static_assert(false,
            "element_visitor: visit overload not provided for type: See name "
            "in reveal below:");
        RevealType<UnhandledType> reveal;
    }
};

template <typename Derived>
struct element_children
{
    element_children() = default;

    template <typename... Args,
        typename = std::enable_if_t<(is_valid_child_v<Derived, std::decay_t<Args>> && ...)>>
    explicit element_children(Args &&...args)
    {
        (add(std::forward<Args>(args)), ...);
    }

    // Deep copy constructor:
    element_children(const element_children &other)
    {
        for (const auto &child : other.m_children)
            m_children.push_back(child->clone());
    }
    element_children &operator=(const element_children &other)
    {
        if (this != &other)
        {
            m_children.clear();
            for (const auto &child : other.m_children)
                m_children.push_back(child->clone());
        }
        return *this;
    }
    element_children(element_children &&) = default;
    element_children &operator=(element_children &&) = default;
    template <typename Child>
    void add(Child &&child)
    {
        using DecayChild = std::decay_t<Child>;
        static_assert(is_valid_child_v<Derived, DecayChild>, "Invalid child type");
        m_children.emplace_back(std::make_unique<DecayChild>(std::forward<Child>(child)));
    }

    template <typename Child>
    void add(const Child &child)
    {
        static_assert(is_valid_child_v<Derived, Child>, "Invalid child type");
        m_children.push_back(std::make_unique<Child>(child));
    }

    auto begin() const { return m_children.begin(); }
    auto end() const { return m_children.end(); }

    std::list<std::unique_ptr<element>> m_children;
};

template <typename Derived>
struct element_base : public element
{
    void accept(element_visitor &visitor) const override
    {
        visitor.visit(static_cast<const Derived &>(*this));
    }

    std::unique_ptr<element> clone() const override
    {
        return std::make_unique<Derived>(static_cast<const Derived &>(*this));
    }
};

// template<typename Derived>
// struct element_base : public element_children<Derived>, public element_visitor_mixin<Derived>
//{
// };

}