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

// Private: the Open Packaging Conventions container of the Office Open XML
// formats (.docx): a zip of parts, their content types in
// [Content_Types].xml, and relationships in _rels/*.rels parts.

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace docsmith::opc
{

struct relationship
{
    std::string id;
    std::string type;
    std::string target; //!< As written: relative to the source part's folder, or a URL
    bool external{false};
    bool operator==(const relationship &) const = default;
};

/// An OPC package held entirely in memory. Part names have no leading '/'
/// ("word/document.xml"). Relationship parts aren't parts here: each part's
/// relationships are kept apart, by the name of the part they belong to ("" for
/// the package's own, _rels/.rels), and written out from there.
class package
{
public:
    struct part
    {
        std::string data;
        std::string content_type;
    };

    static package read(const std::filesystem::path &p);
    static package read_bytes(std::string bytes);

    /// [Content_Types].xml first, then the parts in name order, each part's
    /// relationships after it.
    void write(const std::filesystem::path &p) const;
    std::string write_bytes() const;

    const part *find(const std::string &name) const;
    bool contains(const std::string &name) const { return find(name) != nullptr; }
    void set(const std::string &name, std::string data, std::string content_type);

    /// Removes a part (and its relationships) and returns it, or nullopt.
    std::optional<part> take(const std::string &name);

    const std::map<std::string, part> &parts() const { return m_parts; }

    /// The relationships of the part source ("" for the package).
    std::vector<relationship> &rels(const std::string &source) { return m_rels[source]; }
    const std::vector<relationship> *find_rels(const std::string &source) const;

    /// The first relationship of source with that type, or nullptr.
    const relationship *find_rel_by_type(const std::string &source, const std::string &type) const;
    const relationship *find_rel(const std::string &source, const std::string &id) const;

    /// The id of a relationship from source to target (external or not) of
    /// that type, adding one if there's none.
    std::string ensure_rel(const std::string &source, const std::string &type,
        const std::string &target, bool external = false);

    /// The name of the part an internal relationship of source points to.
    static std::string resolve(const std::string &source, const std::string &target);
    /// The target to write in source's relationships for the part name.
    static std::string relative(const std::string &source, const std::string &name);

private:
    std::map<std::string, part> m_parts;
    std::map<std::string, std::vector<relationship>> m_rels;
};

} // namespace docsmith::opc
