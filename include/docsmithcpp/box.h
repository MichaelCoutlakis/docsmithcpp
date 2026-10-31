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

#include <concepts>
#include <stdexcept>
#include <utility>

namespace docsmith
{

// ============================================================================
//  box<T> - a heap-allocated, value-semantic holder used to break the recursion
//  in the node variants (a span contains inline_nodes, which contain spans...).
//
//  Why this exists: std::variant requires its alternatives to be complete, and
//  some standard libraries (notably libc++) instantiate an alternative's
//  destructor/copy while forming the variant. If a node held its recursive
//  children directly by value (vector<inline_node> inside span, where span is
//  an inline_node alternative), that instantiation would need the *incomplete*
//  node type and fail to compile. MSVC and libstdc++ defer it and accept the
//  by-value form, but libc++ does not -- so for portability the recursive edge
//  goes through box.
//
//  The trick that makes box work with an incomplete T: the destroy / clone /
//  compare operations are type-erased function pointers captured in the value
//  constructor, which is only ever instantiated where T is complete (the call
//  site). box's own destructor, copy, move and operator== therefore never
//  require T to be complete, so the variant can be formed freely.
// ============================================================================
template <typename T>
class box
{
public:
    box() :
        m_ptr(new T()), m_vtable(make_vtable())
    {
    }
    box(const T &value) :
        box(value, tag{})
    {
    }
    box(T &&value) :
        box(std::move(value), tag{})
    {
    }

    box(const box &other) :
        m_ptr(other.m_ptr ? static_cast<T *>(other.m_vtable->clone(other.m_ptr)) : nullptr),
        m_vtable(other.m_vtable)
    {
    }
    box(box &&other) noexcept :
        m_ptr(other.m_ptr), m_vtable(other.m_vtable)
    {
        other.m_ptr = nullptr;
    }
    box &operator=(const box &other)
    {
        box tmp(other);
        swap(tmp);
        return *this;
    }
    box &operator=(box &&other) noexcept
    {
        swap(other);
        return *this;
    }
    ~box()
    {
        if(m_ptr)
            m_vtable->destroy(m_ptr);
    }

    T &operator*() noexcept { return *m_ptr; }
    const T &operator*() const noexcept { return *m_ptr; }
    T *operator->() noexcept { return m_ptr; }
    const T *operator->() const noexcept { return m_ptr; }

    bool operator==(const box &other) const
    {
        if(m_ptr == other.m_ptr)
            return true;
        if(!m_ptr || !other.m_ptr)
            return false;
        if(!m_vtable->equal)
            throw std::logic_error("box: the held type has no operator==");
        return m_vtable->equal(m_ptr, other.m_ptr);
    }

private:
    struct vtable
    {
        void *(*clone)(const void *);
        void (*destroy)(void *) noexcept;
        bool (*equal)(const void *, const void *);
    };

    struct tag
    {
    };

    template <typename U>
    box(U &&value, tag) :
        m_ptr(new T(std::forward<U>(value))), m_vtable(make_vtable())
    {
    }

    void swap(box &other) noexcept
    {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_vtable, other.m_vtable);
    }

    // Instantiated only here, where T is necessarily complete. Comparison is
    // captured only if T has an operator== (a pimpl, for example, may not).
    static const vtable *make_vtable()
    {
        static const vtable vt{
            [](const void *p) -> void * { return new T(*static_cast<const T *>(p)); },
            [](void *p) noexcept { delete static_cast<T *>(p); },
            equal_fn(),
        };
        return &vt;
    }

    static constexpr bool (*equal_fn())(const void *, const void *)
    {
        if constexpr(std::equality_comparable<T>)
            return [](const void *a, const void *b)
            { return *static_cast<const T *>(a) == *static_cast<const T *>(b); };
        else
            return nullptr;
    }

    // Typed (not void *) so debuggers can follow it to the held value. A pointer
    // to an incomplete T is fine, and converting it to and from void * for the
    // vtable calls needs no complete type either.
    T *m_ptr;
    const vtable *m_vtable;
};

} // namespace docsmith
