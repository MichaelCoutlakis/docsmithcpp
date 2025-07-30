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
#include <tuple>
#include <type_traits>

#include <iostream>

#include "docsmithcpp/element.h"

namespace docsmith
{
template <typename... ChildTypes>
struct kids
{
    kids(ChildTypes &&...children) :
        m_child_content(std::forward<ChildTypes>(children)...)
    {
    }
    std::tuple<ChildTypes...> m_child_content;
};

// Tag for adding kids, use an object of this type when constructor could be confused, for example,
// adding a single span to a span. To make it more concise, constexpr auto ins = kids_tag{}.
struct kids_tag
{
};

// Deduction guide for kids:
template <typename... ChildTypes>
kids(ChildTypes &&...) -> kids<ChildTypes...>;

void add_text(std::list<std::unique_ptr<element>> &dest, std::string t);

template <typename Derived>
struct element_children : virtual element
{
    element_children() { std::cout << "default constructor\n"; }

    template <typename... Args, // clang-format off
         typename = std::enable_if_t<(
             (is_valid_child_v<Derived, std::decay_t<Args>> ||
              std::is_convertible_v<std::decay_t<Args>, std::string>)
             && ...)>> // clang-format on
    explicit element_children(Args &&...args)
    {
        std::cout << "constructor\n";
        (add(std::forward<Args>(args)), ...);
    }

    template <typename... Args, // clang-format off
         typename = std::enable_if_t<(
             (is_valid_child_v<Derived, std::decay_t<Args>> ||
              std::is_convertible_v<std::decay_t<Args>, std::string>)
             && ...)>> // clang-format on
    explicit element_children(kids_tag tg, Args &&...args)
    {
        std::cout << "tag constructor\n";
        (add(std::forward<Args>(args)), ...);
    }

    template <typename... ChildTypes>
    element_children(kids<ChildTypes...> &&content)
    {
        std::apply([this](auto &&...args) { (add(std::forward<decltype(args)>(args)), ...); },
            content.m_child_content);
    }

    // Deep copy constructor:
    element_children(const element_children &other)
    {
        std::cout << "copy constructor\n";
        for(const auto &child : other.m_children)
            m_children.push_back(child->clone());
    }
    element_children &operator=(const element_children &other)
    {
        std::cout << "copy assignment operator\n";
        if(this != &other)
        {
            m_children.clear();
            for(const auto &child : other.m_children)
                m_children.push_back(child->clone());
        }
        return *this;
    }

    element_children(element_children &&other) { m_children = std::move(other.m_children); }
    element_children &operator=(element_children &&other)
    {
        if(this != &other)
        {
            m_children = std::move(other.m_children);
        }
        return *this;
    }

    template <typename... Args, // clang-format off
        typename = std::enable_if_t<(
            (is_valid_child_v<Derived, std::decay_t<Args>> ||
             std::is_convertible_v<std::decay_t<Args>, std::string>)
            && ...)>> // clang-format on
    Derived &set_content(Args &&...args)
    {
        (add(std::forward<Args>(args)), ...);
    }

    template <typename Child>
    Derived &add(Child &&child)
    {
        using DecayChild = std::decay_t<Child>;
        static_assert(is_valid_child_v<Derived, DecayChild>, "Invalid child type");
        using ChildType = decltype(child);

        if constexpr(std::is_lvalue_reference_v<ChildType>)
        {
            // Have to copy then use the move constructor, if we use the constructor it will call
            // add which will be recursive...
            auto child_copy = child;
            m_children.emplace_back(std::make_unique<DecayChild>(std::move(child_copy)));
        }
        else if constexpr(std::is_rvalue_reference_v<ChildType>)
        {
            auto p = std::make_unique<DecayChild>(std::forward<Child>(child));
            m_children.emplace_back(std::move(p));
        }
        else
            static_assert(false, "Unhandled reference type");

        return *static_cast<Derived *>(this);
    }

    Derived &add(const char *t)
    {
        static_assert(
            is_valid_child_v<Derived, text>, "text is not a child type - can't add string literal");

        // return add(text(t));
        add_text(m_children, t);
        return *static_cast<Derived *>(this);
    }
    Derived &add(const std::string &t)
    {
        static_assert(
            is_valid_child_v<Derived, text>, "text is not a child type - can't add string");

        // return add(text(t));
        add_text(m_children, t);
        return *static_cast<Derived *>(this);
    }

    void add_child(std::unique_ptr<element> child) override
    {
        using U = std::remove_reference_t<decltype(*child)>;
        // static_assert(is_valid_child_v<Derived, U>, "Invalid child type");
        m_children.push_back(std::move(child));
    }

    auto begin() const { return m_children.begin(); }
    auto end() const { return m_children.end(); }

    std::list<std::unique_ptr<element>> m_children;
};

// clang-format off
// Helpers to determine if the class heirarchy has element_children mixed in:
template<typename NodeType>
struct is_element_children : std::is_base_of<element_children<NodeType>, NodeType> {};

template<typename NodeType>
inline constexpr bool has_children_v = is_element_children<NodeType>::value;
// clang-format on

template <typename Derived>
struct element_base : virtual element
{
    void accept(element_visitor &visitor) const override
    {
        visitor.visit(static_cast<const Derived &>(*this));

        if constexpr(has_children_v<Derived>)
        {
            visitor.push();
            const auto &self = static_cast<const Derived &>(*this);
            for(const auto &child : self)
                child->accept(visitor);
            visitor.pop();
        }
    }

    std::unique_ptr<element> clone() const override
    {
        return std::make_unique<Derived>(static_cast<const Derived &>(*this));
    }

    bool is_equal(const element &other) const override
    {
        if(auto p = dynamic_cast<const Derived *>(&other))
        {
            return static_cast<const Derived &>(*this) == *p;
        }

        return false;
    }
};

inline bool compare_equality(
    const std::list<std::unique_ptr<element>> &lhs, const std::list<std::unique_ptr<element>> &rhs)
{
    if(lhs.size() != rhs.size())
        return false;

    for(auto it_lhs = std::cbegin(lhs), it_rhs = std::cbegin(rhs); it_lhs != std::cend(lhs);
        std::advance(it_lhs, 1), std::advance(it_rhs, 1))
    {
        if(!(*it_lhs)->is_equal(*it_rhs->get()))
            return false;
    }
    return true;
}
}