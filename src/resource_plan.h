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

// Private: where a text file's images go when it is saved with its images
// next to it (Markdown; HTML has its own copy for now).

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "docsmithcpp/text_doc.h"

namespace docsmith::detail
{

/// Where each image goes and what its path in the file is. Document resources
/// go to resource_dir / name. Files on disk already inside resource_dir are
/// referenced in place; other files are copied to resource_dir / Pictures/,
/// renamed if another image already has that name. Other uris (URLs) are left
/// as is.
struct resource_plan
{
    std::map<std::string, std::string> src; //!< image uri -> path written in the file
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> copy; //!< from, to

    /// base: the folder of the file being saved, or null when writing to a
    /// string (then src is what save() would write, but no copies are planned).
    resource_plan(const text_doc &doc, const std::filesystem::path &resource_dir,
        const std::filesystem::path *base);

    const std::string &src_of(const std::string &uri) const
    {
        auto it = src.find(uri);
        return it != src.end() ? it->second : uri;
    }

    /// Writes the document's resources and copies the planned files.
    void write(const text_doc &doc, const std::filesystem::path &base,
        const std::filesystem::path &resource_dir) const;
};

} // namespace docsmith::detail
