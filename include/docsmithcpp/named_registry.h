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
#include <cstddef>
#include <map>
#include <utility>

namespace docsmith
{

/// Definitions keyed by their name(), in name order: a std::map that takes
/// the key from the value. T has a name() returning a Key.
template <typename Key, typename T>
class named_registry
{
public:
    using key_type = Key;
    using mapped_type = T;
    using container_type = std::map<Key, T>;
    using value_type = typename container_type::value_type;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

    /// Adds value unless one of its name is there, which is kept, as
    /// std::map::insert. The bool says whether value was added.
    std::pair<iterator, bool> insert(T value)
    {
        Key key = value.name();
        return m_data.try_emplace(std::move(key), std::move(value));
    }
    /// Adds value, replacing any of its name, as std::map::insert_or_assign.
    /// The bool says whether it was added rather than replaced.
    std::pair<iterator, bool> insert_or_assign(T value)
    {
        Key key = value.name();
        return m_data.insert_or_assign(std::move(key), std::move(value));
    }
    /// The definition of that name, added (empty) if there's none, as
    /// std::map::operator[].
    T &operator[](const Key &name)
        requires std::constructible_from<T, const Key &>
    { return m_data.try_emplace(name, name).first->second; }

    /// The definition of that name, or nullptr.
    [[nodiscard]] T *find(const Key &name)
    {
        auto it = m_data.find(name);
        return it != m_data.end() ? &it->second : nullptr;
    }
    [[nodiscard]] const T *find(const Key &name) const
    {
        auto it = m_data.find(name);
        return it != m_data.end() ? &it->second : nullptr;
    }
    bool contains(const Key &name) const { return m_data.contains(name); }
    /// Removes the definition of that name; returns how many were removed.
    std::size_t erase(const Key &name) { return m_data.erase(name); }

    iterator begin() noexcept { return m_data.begin(); }
    iterator end() noexcept { return m_data.end(); }
    const_iterator begin() const noexcept { return m_data.begin(); }
    const_iterator end() const noexcept { return m_data.end(); }

    std::size_t size() const noexcept { return m_data.size(); }
    bool empty() const noexcept { return m_data.empty(); }

    bool operator==(const named_registry &) const = default;

private:
    container_type m_data;
};

} // namespace docsmith
