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
#include "opc/package.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <pugixml.hpp>

#include "bytes.h"
#include "zip/archive.h"

namespace docsmith::opc
{
namespace
{
constexpr const char *content_types_name = "[Content_Types].xml";
constexpr const char *rels_content_type =
    "application/vnd.openxmlformats-package.relationships+xml";
constexpr const char *content_types_ns =
    "http://schemas.openxmlformats.org/package/2006/content-types";
constexpr const char *rels_ns = "http://schemas.openxmlformats.org/package/2006/relationships";

/// The element's name without its prefix. These two parts have one namespace
/// each, so the local name is enough to read them whatever the prefix.
std::string_view local_name(const pugi::xml_node &n)
{
    std::string_view s = n.name();
    const auto colon = s.find(':');
    return colon == std::string_view::npos ? s : s.substr(colon + 1);
}

std::string lower(std::string s)
{
    std::transform(s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string extension_of(const std::string &name)
{
    const auto slash = name.rfind('/');
    const auto dot = name.rfind('.');
    if(dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return {};
    return lower(name.substr(dot + 1));
}

/// "word/_rels/document.xml.rels" for "word/document.xml", "_rels/.rels" for "".
std::string rels_name(const std::string &source)
{
    const auto slash = source.rfind('/');
    const std::string dir = slash == std::string::npos ? "" : source.substr(0, slash + 1);
    const std::string file = slash == std::string::npos ? source : source.substr(slash + 1);
    return dir + "_rels/" + file + ".rels";
}

/// The inverse of rels_name(), or nullopt if name isn't a relationship part.
std::optional<std::string> source_of_rels(const std::string &name)
{
    if(!name.ends_with(".rels"))
        return std::nullopt;
    const auto pos = name.rfind("_rels/");
    if(pos == std::string::npos || (pos != 0 && name[pos - 1] != '/'))
        return std::nullopt;
    const std::string file = name.substr(pos + 6, name.size() - pos - 6 - 5);
    if(file.find('/') != std::string::npos)
        return std::nullopt;
    return name.substr(0, pos) + file;
}

std::vector<relationship> parse_rels(const std::string &data)
{
    std::vector<relationship> out;
    pugi::xml_document xml;
    if(!xml.load_buffer(data.data(), data.size()))
        return out;
    for(auto r : xml.document_element().children())
        if(local_name(r) == "Relationship")
            out.push_back(relationship{r.attribute("Id").as_string(),
                r.attribute("Type").as_string(),
                r.attribute("Target").as_string(),
                std::string_view(r.attribute("TargetMode").as_string()) == "External"});
    return out;
}

std::string save(const pugi::xml_document &xml)
{
    std::ostringstream ss;
    xml.save(ss, "", pugi::format_raw);
    return ss.str();
}

pugi::xml_document declared()
{
    pugi::xml_document xml;
    auto decl = xml.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";
    decl.append_attribute("standalone") = "yes";
    return xml;
}

std::string rels_xml(const std::vector<relationship> &rels)
{
    auto xml = declared();
    auto root = xml.append_child("Relationships");
    root.append_attribute("xmlns") = rels_ns;
    for(const auto &r : rels)
    {
        auto n = root.append_child("Relationship");
        n.append_attribute("Id") = r.id.c_str();
        n.append_attribute("Type") = r.type.c_str();
        n.append_attribute("Target") = r.target.c_str();
        if(r.external)
            n.append_attribute("TargetMode") = "External";
    }
    return save(xml);
}

std::vector<std::string> split_path(std::string_view s)
{
    std::vector<std::string> out;
    while(!s.empty())
    {
        const auto slash = s.find('/');
        out.emplace_back(s.substr(0, slash));
        if(slash == std::string_view::npos)
            break;
        s.remove_prefix(slash + 1);
    }
    return out;
}
} // namespace

package package::read(const std::filesystem::path &p)
{
    try
    {
        return read_bytes(detail::read_bytes(p));
    }
    catch(const std::runtime_error &e)
    {
        throw std::runtime_error(detail::utf8(p) + ": " + e.what());
    }
}

package package::read_bytes(std::string bytes)
{
    package pkg;
    std::string content_types;
    std::map<std::string, std::string> rels;
    for(auto &e : zip::read(bytes))
    {
        if(e.name.ends_with('/'))
            continue;
        if(e.name == content_types_name)
            content_types = std::move(e.data);
        else if(auto source = source_of_rels(e.name))
            rels[*source] = std::move(e.data);
        else
            pkg.m_parts[e.name] = part{std::move(e.data)};
    }
    if(content_types.empty())
        throw std::runtime_error("Not an OPC package: no [Content_Types].xml");

    std::map<std::string, std::string> defaults, overrides;
    pugi::xml_document xml;
    if(xml.load_buffer(content_types.data(), content_types.size()))
        for(auto c : xml.document_element().children())
        {
            if(local_name(c) == "Default")
                defaults[lower(c.attribute("Extension").as_string())] =
                    c.attribute("ContentType").as_string();
            else if(local_name(c) == "Override")
            {
                std::string name = c.attribute("PartName").as_string();
                if(name.starts_with('/'))
                    name.erase(0, 1);
                overrides[name] = c.attribute("ContentType").as_string();
            }
        }
    for(auto &[name, pt] : pkg.m_parts)
    {
        if(auto it = overrides.find(name); it != overrides.end())
            pt.content_type = it->second;
        else if(auto d = defaults.find(extension_of(name)); d != defaults.end())
            pt.content_type = d->second;
    }
    for(auto &[source, data] : rels)
        pkg.m_rels[source] = parse_rels(data);
    return pkg;
}

void package::write(const std::filesystem::path &p) const { detail::write_bytes(p, write_bytes()); }

std::string package::write_bytes() const
{
    // A Default for each extension whose parts all have one content type (and
    // for .rels and .xml); an Override for every other part.
    std::map<std::string, std::string> defaults{{"rels", rels_content_type}};
    std::map<std::string, bool> mixed;
    for(const auto &[name, pt] : m_parts)
    {
        const std::string ext = extension_of(name);
        if(ext.empty() || ext == "rels")
            continue;
        if(auto [it, added] = defaults.emplace(ext, pt.content_type);
            !added && it->second != pt.content_type)
            mixed[ext] = true;
    }
    // Most .xml parts have content types of their own, so .xml is the generic one.
    if(mixed.contains("xml") || defaults.contains("xml"))
    {
        defaults["xml"] = "application/xml";
        mixed.erase("xml");
    }
    for(const auto &[ext, m] : mixed)
        defaults.erase(ext);

    auto xml = declared();
    auto types = xml.append_child("Types");
    types.append_attribute("xmlns") = content_types_ns;
    for(const auto &[ext, ct] : defaults)
    {
        auto d = types.append_child("Default");
        d.append_attribute("Extension") = ext.c_str();
        d.append_attribute("ContentType") = ct.c_str();
    }
    for(const auto &[name, pt] : m_parts)
    {
        auto d = defaults.find(extension_of(name));
        if(d != defaults.end() && d->second == pt.content_type)
            continue;
        auto o = types.append_child("Override");
        o.append_attribute("PartName") = ("/" + name).c_str();
        o.append_attribute("ContentType") = pt.content_type.c_str();
    }

    zip::writer zip;
    zip.add(content_types_name, save(xml));
    auto add_rels = [&](const std::string &source)
    {
        if(auto it = m_rels.find(source); it != m_rels.end() && !it->second.empty())
            zip.add(rels_name(source), rels_xml(it->second));
    };
    add_rels("");
    for(const auto &[name, pt] : m_parts)
    {
        zip.add(name, pt.data);
        add_rels(name);
    }
    return zip.finish();
}

const package::part *package::find(const std::string &name) const
{
    auto it = m_parts.find(name);
    return it != m_parts.end() ? &it->second : nullptr;
}

void package::set(const std::string &name, std::string data, std::string content_type)
{ m_parts[name] = part{std::move(data), std::move(content_type)}; }

std::optional<package::part> package::take(const std::string &name)
{
    auto node = m_parts.extract(name);
    if(node.empty())
        return std::nullopt;
    m_rels.erase(name);
    return std::move(node.mapped());
}

const std::vector<relationship> *package::find_rels(const std::string &source) const
{
    auto it = m_rels.find(source);
    return it != m_rels.end() ? &it->second : nullptr;
}

const relationship *package::find_rel_by_type(
    const std::string &source, const std::string &type) const
{
    if(const auto *rels = find_rels(source))
        for(const auto &r : *rels)
            if(r.type == type)
                return &r;
    return nullptr;
}

const relationship *package::find_rel(const std::string &source, const std::string &id) const
{
    if(const auto *rels = find_rels(source))
        for(const auto &r : *rels)
            if(r.id == id)
                return &r;
    return nullptr;
}

std::string package::ensure_rel(
    const std::string &source, const std::string &type, const std::string &target, bool external)
{
    auto &rels = m_rels[source];
    for(const auto &r : rels)
        if(r.type == type && r.target == target && r.external == external)
            return r.id;
    std::string id;
    for(int i = static_cast<int>(rels.size()) + 1;; ++i)
    {
        id = "rId" + std::to_string(i);
        if(std::none_of(
               rels.begin(), rels.end(), [&](const relationship &r) { return r.id == id; }))
            break;
    }
    rels.push_back(relationship{id, type, target, external});
    return id;
}

std::string package::resolve(const std::string &source, const std::string &target)
{
    std::vector<std::string> parts;
    if(!target.starts_with('/'))
    {
        parts = split_path(source);
        if(!parts.empty())
            parts.pop_back(); // the source's own file name
    }
    for(auto &seg : split_path(target))
    {
        if(seg == "..")
        {
            if(!parts.empty())
                parts.pop_back();
        }
        else if(seg != "." && !seg.empty())
            parts.push_back(seg);
    }
    std::string out;
    for(const auto &seg : parts)
        out += (out.empty() ? "" : "/") + seg;
    return out;
}

std::string package::relative(const std::string &source, const std::string &name)
{
    auto from = split_path(source);
    if(!from.empty())
        from.pop_back();
    const auto to = split_path(name);
    std::size_t common = 0;
    while(common < from.size() && common + 1 < to.size() && from[common] == to[common])
        ++common;
    std::string out;
    for(std::size_t i = common; i < from.size(); ++i)
        out += "../";
    for(std::size_t i = common; i < to.size(); ++i)
        out += to[i] + (i + 1 < to.size() ? "/" : "");
    return out;
}

} // namespace docsmith::opc
