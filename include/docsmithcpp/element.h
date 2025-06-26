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

    template <typename Unhandled>
    void visit(const Unhandled &value)
    {
    }
};

template <typename Derived>
struct element_children : virtual element
{
    // element_children() = default;
    element_children() { }

    template <typename... Args,
        typename = std::enable_if_t<(is_valid_child_v<Derived, std::decay_t<Args>> && ...)>>
    explicit element_children(Args &&...args)
    {
        (add(std::forward<Args>(args)), ...);
    }

    explicit element_children(const char *t)
    {
        add_child(std::make_unique<text>(t));
        // add(t);
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

    // template<typename StringLike>
    // std::enable_if_t<std::is_convertible_v<StringLike, std::string>> add(StringLike
    // &&string_like)
    //{
    //     static_assert(is_valid_child_v<Derived, text>, "Invalid child type");
    //     add_child(std::make_unique<text>(std::forward<StringLike>(string_like)));
    // }

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

template <typename Derived>
struct element_base : virtual element
{
    void accept(element_visitor &visitor) const override
    {
        visitor.visit(static_cast<const Derived &>(*this));
    }

    std::unique_ptr<element> clone() const override
    {
        return std::make_unique<Derived>(static_cast<const Derived &>(*this));
    }

    bool is_equal(const element &other) const override
    {
        if (auto p = dynamic_cast<const Derived *>(&other))
        {
            return static_cast<const Derived &>(*this) == *p;
        }

        return false;
    }
};

inline bool compare_equality(
    const std::list<std::unique_ptr<element>> &lhs, const std::list<std::unique_ptr<element>> &rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (auto it_lhs = std::cbegin(lhs), it_rhs = std::cbegin(rhs); it_lhs != std::cend(lhs);
         std::advance(it_lhs, 1), std::advance(it_rhs, 1))
    {
        if (!(*it_lhs)->is_equal(*it_rhs->get()))
            return false;
    }
    return true;
}

}