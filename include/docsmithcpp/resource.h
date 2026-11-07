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

// Binary data a document refers to by name, such as an image.

#include <memory>
#include <string>

#include "docsmithcpp/named_registry.h"

namespace docsmith
{

/// A named blob of bytes plus its media type. The name is a relative path
/// ("Pictures/logo.png"); an image whose uri equals it refers to it. The bytes
/// are shared and read-only, so copying a resource (or a document) is cheap.
class resource
{
public:
    resource() = default;
    resource(std::string name, std::string data, std::string media_type) :
        m_name(std::move(name)),
        m_data(std::make_shared<const std::string>(std::move(data))),
        m_media_type(std::move(media_type))
    {
    }

    const std::string &name() const { return m_name; }
    const std::string &data() const { return m_data ? *m_data : empty(); }
    const std::string &media_type() const { return m_media_type; }

    /// Compares the bytes, not where they are held.
    bool operator==(const resource &rhs) const
    {
        return m_name == rhs.m_name && m_media_type == rhs.m_media_type &&
            (m_data == rhs.m_data || data() == rhs.data());
    }

private:
    static const std::string &empty()
    {
        static const std::string e;
        return e;
    }

    std::string m_name;
    std::shared_ptr<const std::string> m_data;
    std::string m_media_type;
};

using resource_registry = named_registry<std::string, resource>;

} // namespace docsmith
