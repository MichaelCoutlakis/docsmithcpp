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

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace docsmith
{

/// Properties of the document as a whole (Dublin Core, as ODT, DOCX, PDF and
/// HTML all use). None of it is shown on the page: a visible title is a
/// paragraph in the body.
///
///     doc.meta.title = "Annual Report";
///     doc.meta = {.title = "Annual Report", .author = "A. Writer", .language = "en-ZA"};
struct metadata
{
    std::string title;
    std::string author;      //!< Several authors go in one string, e.g. "A; B"
    std::string language;    //!< BCP 47 tag, e.g. "en-ZA"
    std::string subject;
    std::string description; //!< A summary or abstract
    std::vector<std::string> keywords;
    std::optional<std::chrono::sys_seconds> created;
    std::optional<std::chrono::sys_seconds> modified;
    std::map<std::string, std::string> custom; //!< User-defined properties, by name

    bool operator==(const metadata &) const = default;
};

} // namespace docsmith
