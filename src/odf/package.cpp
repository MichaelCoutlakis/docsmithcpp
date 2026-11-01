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
#include "odf/package.h"

#include <sstream>
#include <stdexcept>

#include <pugixml.hpp>

#include "bytes.h"
#include "zip/archive.h"

namespace docsmith::odf
{
namespace fs = std::filesystem;

namespace
{
constexpr const char *manifest_path = "META-INF/manifest.xml";
}

package package::read(const fs::path &p)
{
    std::vector<zip::entry> entries;
    try
    {
        entries = zip::read(detail::read_bytes(p));
    }
    catch(const std::runtime_error &e)
    {
        throw std::runtime_error(detail::utf8(p) + ": " + e.what());
    }

    package pkg;
    std::string manifest;
    for(auto &e : entries)
    {
        if(e.name == "mimetype")
            pkg.m_mimetype = std::move(e.data);
        else if(e.name == manifest_path)
            manifest = std::move(e.data);
        else
            pkg.m_entries[e.name] = entry{std::move(e.data)};
    }

    // Attach the manifest's media types. Directories may be listed without a
    // zip entry of their own; listed files that don't exist are ignored.
    pugi::xml_document xml;
    if(!manifest.empty() && xml.load_buffer(manifest.data(), manifest.size()))
    {
        auto root = xml.child("manifest:manifest");
        if(auto v = root.attribute("manifest:version"))
            pkg.m_version = v.as_string();

        for(auto fe : root.children("manifest:file-entry"))
        {
            const std::string path = fe.attribute("manifest:full-path").as_string();
            if(path == "/")
                continue;
            auto it = pkg.m_entries.find(path);
            if(it == pkg.m_entries.end() && path.ends_with('/'))
                it = pkg.m_entries.emplace(path, entry{}).first;
            if(it != pkg.m_entries.end())
            {
                it->second.media_type = fe.attribute("manifest:media-type").as_string();
                it->second.in_manifest = true;
            }
        }
    }
    return pkg;
}

void package::write(const fs::path &p) const
{
    zip::writer zip;

    // ODF: the mimetype must be the first entry, stored uncompressed.
    zip.add("mimetype", m_mimetype, zip::method::store);
    for(const auto &[name, e] : m_entries)
        zip.add(name, e.data);
    zip.add(manifest_path, manifest_xml());

    detail::write_bytes(p, zip.finish());
}

const package::entry *package::find(const std::string &name) const
{
    auto it = m_entries.find(name);
    return it != m_entries.end() ? &it->second : nullptr;
}

void package::set(const std::string &name, std::string data, std::string media_type)
{
    m_entries[name] = entry{std::move(data), std::move(media_type), true};
}

std::optional<package::entry> package::take(const std::string &name)
{
    auto node = m_entries.extract(name);
    if(node.empty())
        return std::nullopt;
    return std::move(node.mapped());
}

std::string package::manifest_xml() const
{
    pugi::xml_document xml;
    auto decl = xml.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    auto root = xml.append_child("manifest:manifest");
    root.append_attribute("xmlns:manifest") = "urn:oasis:names:tc:opendocument:xmlns:manifest:1.0";
    root.append_attribute("manifest:version") = m_version.c_str();

    auto add = [&root](const std::string &path, const std::string &media_type)
    {
        auto fe = root.append_child("manifest:file-entry");
        fe.append_attribute("manifest:full-path") = path.c_str();
        fe.append_attribute("manifest:media-type") = media_type.c_str();
        return fe;
    };
    add("/", m_mimetype).append_attribute("manifest:version") = m_version.c_str();
    for(const auto &[name, e] : m_entries)
        if(e.in_manifest)
            add(name, e.media_type);

    std::ostringstream ss;
    xml.save(ss, " ");
    return ss.str();
}

} // namespace docsmith::odf
