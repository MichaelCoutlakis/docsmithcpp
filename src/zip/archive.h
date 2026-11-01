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

// Private: a zip archive read from and written to bytes in memory, with no
// knowledge of what is inside. The ODF package (and later OPC, for DOCX) sit on
// top of it.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace docsmith::zip
{

enum class method
{
    store,
    deflate,
};

struct entry
{
    std::string name; //!< Path in the archive; ends in '/' for a directory
    std::string data;
    method m{method::deflate};
};

/// All entries in archive order.
std::vector<entry> read(std::string_view bytes);

/// Builds an archive in memory. Entries are written in the order they are
/// added.
class writer
{
public:
    writer();
    ~writer();
    writer(const writer &) = delete;
    writer &operator=(const writer &) = delete;

    /// Add an entry. A name ending in '/' is a directory and takes no data.
    void add(const std::string &name, std::string_view data, method m = method::deflate);

    /// The finished archive. No entries can be added after this.
    std::string finish();

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace docsmith::zip
