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

#include <string>
#include <unordered_map>
#include <utility>

template <typename T, std::string (*GetName)(const T &)>
class named_registry
{
public:
    using key_type = std::string;
    using mapped_type = T;
    using container_type = std::unordered_map<key_type, mapped_type>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

    mapped_type &add(const mapped_type &value)
    {
        auto key = GetName(value);
        auto [it, insrt_op] = m_data.insert_or_assign(std::move(key), value);
        return it->second;
    }
    mapped_type *find(const key_type &name)
    {
        auto it = m_data.find(name);
        return it != m_data.end() ? &it->second : nullptr;
    }

    iterator begin() noexcept { return m_data.begin(); }
    iterator end() noexcept { return m_data.end(); }
    const_iterator begin() const noexcept { return m_data.begin(); }
    const_iterator end() const noexcept { return m_data.end(); }

    std::size_t size() const noexcept { return m_data.size(); }
    bool empty() const noexcept { return m_data.empty(); }

    private:
    std::unordered_map<key_type, mapped_type> m_data;
};
