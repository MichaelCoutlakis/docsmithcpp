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

// Private: the zip container shared by the ODF formats (.odt, .ods, .odp).

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace docsmith::odf
{

/// An ODF package held entirely in memory: the mimetype, the bytes of every
/// other entry, and the media types the manifest lists for them. read() and
/// write() are the only calls that touch the filesystem, so nothing stays open
/// and a loaded package can be written back over its own source file.
class package
{
public:
    struct entry
    {
        std::string data;        //!< Raw bytes (empty for a directory entry)
        std::string media_type;  //!< As listed in META-INF/manifest.xml
        bool in_manifest{false}; //!< Whether the manifest lists this entry
    };

    explicit package(std::string mimetype = {}) :
        m_mimetype(std::move(mimetype))
    {
    }

    static package read(const std::filesystem::path &p);

    /// Write the archive: mimetype first and uncompressed (as ODF requires), the
    /// other entries deflated, and a manifest regenerated from the entries.
    void write(const std::filesystem::path &p) const;

    const std::string &mimetype() const { return m_mimetype; }
    void set_mimetype(std::string m) { m_mimetype = std::move(m); }

    /// ODF version from the manifest ("1.2" for new packages). The XML parts
    /// must declare the same office:version or LibreOffice rejects the file.
    const std::string &version() const { return m_version; }

    const entry *find(const std::string &name) const;
    bool contains(const std::string &name) const { return find(name) != nullptr; }

    /// Add or replace an entry; it is listed in the manifest with media_type.
    void set(const std::string &name, std::string data, std::string media_type);

    /// Remove an entry and return it, or nullopt if there is none.
    std::optional<entry> take(const std::string &name);

private:
    std::string manifest_xml() const;

    std::string m_mimetype;
    std::string m_version{"1.2"};           //!< manifest:version
    std::map<std::string, entry> m_entries; //!< Everything except mimetype and the manifest
};

} // namespace docsmith::odf
