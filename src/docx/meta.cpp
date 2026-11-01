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
#include "docx/meta.h"

#include <string_view>

#include "docx/ooxml.h"
#include "format.h"

namespace docsmith::docx
{
namespace
{
constexpr const char *core_part = "docProps/core.xml";
constexpr const char *app_part = "docProps/app.xml";
constexpr const char *custom_part = "docProps/custom.xml";
// The property set every custom property is in.
constexpr const char *custom_fmtid = "{D5CDD505-2E9C-101B-9397-08002B2CF9AE}";

/// The part a package relationship of that type points to, or "".
std::string part_of(const opc::package &pkg, const char *type)
{
    const auto *r = pkg.find_rel_by_type("", type);
    return r ? opc::package::resolve("", r->target) : std::string();
}

std::vector<std::string> split_keywords(std::string_view s)
{
    std::vector<std::string> out;
    const char sep = s.find(';') != std::string_view::npos ? ';' : ',';
    while(!s.empty())
    {
        const auto pos = s.find(sep);
        std::string_view k = s.substr(0, pos);
        while(!k.empty() && k.front() == ' ')
            k.remove_prefix(1);
        while(!k.empty() && k.back() == ' ')
            k.remove_suffix(1);
        if(!k.empty())
            out.emplace_back(k);
        if(pos == std::string_view::npos)
            break;
        s.remove_prefix(pos + 1);
    }
    return out;
}

void set_text(pugi::xml_node parent, const char *name, const std::string &value)
{
    if(value.empty())
    {
        while(parent.remove_child(name))
            ;
        return;
    }
    auto n = parent.child(name);
    if(!n)
        n = parent.append_child(name);
    if(value != n.text().get())
        n.text().set(value.c_str());
}

void set_date(
    pugi::xml_node parent, const char *name, const std::optional<std::chrono::sys_seconds> &value)
{
    if(auto n = parent.child(name); n && value && detail::parse_datetime(n.text().get()) == value)
        return;
    set_text(parent, name, value ? detail::format_datetime(*value) : std::string());
    if(auto n = parent.child(name); n && !n.attribute("xsi:type"))
        n.append_attribute("xsi:type") = "dcterms:W3CDTF";
}

void add_part(opc::package &pkg, const char *name, const char *type, const char *content_type,
    const pugi::xml_document &xml)
{
    pkg.set(name, xml::save(xml), content_type);
    pkg.ensure_rel("", type, name);
}

void write_core(const metadata &m, opc::package &pkg)
{
    std::string name = part_of(pkg, rel::core_properties);
    pugi::xml_document xml;
    const auto *existing = name.empty() ? nullptr : pkg.find(name);
    if(!existing || !load(xml, existing->data) || !xml.child("cp:coreProperties"))
    {
        if(m.title.empty() && m.author.empty() && m.language.empty() && m.subject.empty() &&
            m.description.empty() && m.keywords.empty() && !m.created && !m.modified)
            return;
        xml.reset();
        xml.append_child("cp:coreProperties");
        name = core_part;
    }
    auto root = xml.child("cp:coreProperties");
    declare(root, {"cp", "dc", "dcterms", "dcmitype", "xsi"});

    set_text(root, "dc:title", m.title);
    set_text(root, "dc:subject", m.subject);
    set_text(root, "dc:creator", m.author);
    set_text(root, "dc:description", m.description);
    set_text(root, "dc:language", m.language);
    if(split_keywords(root.child("cp:keywords").text().get()) != m.keywords)
    {
        std::string joined;
        for(const auto &k : m.keywords)
            joined += (joined.empty() ? "" : ", ") + k;
        set_text(root, "cp:keywords", joined);
    }
    set_date(root, "dcterms:created", m.created);
    set_date(root, "dcterms:modified", m.modified);

    if(xml.first_child().type() != pugi::node_declaration)
    {
        auto decl = xml.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";
        decl.append_attribute("standalone") = "yes";
    }
    pkg.set(name, xml::save(xml), content_type::core);
    pkg.ensure_rel("", rel::core_properties, name);
}

void write_custom(const std::map<std::string, std::string> &custom, opc::package &pkg)
{
    std::string name = part_of(pkg, rel::custom_properties);
    pugi::xml_document xml;
    const auto *existing = name.empty() ? nullptr : pkg.find(name);
    if(!existing || !load(xml, existing->data) || !xml.child("cup:Properties"))
    {
        if(custom.empty())
            return;
        xml.reset();
        auto root = xml.append_child("cup:Properties");
        declare(root, {"cup", "vt"});
        name = custom_part;
    }
    auto root = xml.child("cup:Properties");

    auto remaining = custom;
    int max_pid = 1;
    for(auto p = root.child("cup:property"); p;)
    {
        const auto next = p.next_sibling("cup:property");
        max_pid = std::max(max_pid, p.attribute("pid").as_int());
        const auto it = remaining.find(p.attribute("name").as_string());
        if(it == remaining.end())
            root.remove_child(p);
        else
        {
            if(it->second != p.first_child().text().get())
            {
                while(p.first_child())
                    p.remove_child(p.first_child());
                p.append_child("vt:lpwstr").text().set(it->second.c_str());
            }
            remaining.erase(it);
        }
        p = next;
    }
    for(const auto &[n, v] : remaining)
    {
        auto p = root.append_child("cup:property");
        p.append_attribute("fmtid") = custom_fmtid;
        p.append_attribute("pid") = ++max_pid;
        p.append_attribute("name") = n.c_str();
        p.append_child("vt:lpwstr").text().set(v.c_str());
    }
    pkg.set(name, xml::save(xml), content_type::custom);
    pkg.ensure_rel("", rel::custom_properties, name);
}
} // namespace

metadata read_meta(const opc::package &pkg)
{
    metadata m;
    pugi::xml_document xml;
    if(const auto *core = pkg.find(part_of(pkg, rel::core_properties));
        core && load(xml, core->data))
    {
        auto root = xml.child("cp:coreProperties");
        m.title = root.child("dc:title").text().get();
        m.subject = root.child("dc:subject").text().get();
        m.author = root.child("dc:creator").text().get();
        m.description = root.child("dc:description").text().get();
        m.language = root.child("dc:language").text().get();
        m.keywords = split_keywords(root.child("cp:keywords").text().get());
        m.created = detail::parse_datetime(root.child("dcterms:created").text().get());
        m.modified = detail::parse_datetime(root.child("dcterms:modified").text().get());
    }
    if(const auto *custom = pkg.find(part_of(pkg, rel::custom_properties));
        custom && load(xml, custom->data))
        for(auto p : xml.child("cup:Properties").children("cup:property"))
            m.custom.emplace(p.attribute("name").as_string(), p.first_child().text().get());
    return m;
}

void write_meta(const metadata &m, opc::package &pkg, bool is_new)
{
    write_core(m, pkg);
    write_custom(m.custom, pkg);
    if(is_new)
    {
        pugi::xml_document xml;
        auto root = xml.append_child("ep:Properties");
        declare(root, {"ep", "vt"});
        root.append_child("ep:Application").text().set("DocSmith++");
        add_part(pkg, app_part, rel::extended_properties, content_type::app, xml);
    }
}

} // namespace docsmith::docx
