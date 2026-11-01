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
#include "odf/meta.h"

#include <cstdio>
#include <sstream>
#include <string_view>

#include <pugixml.hpp>

#include "format.h"
#include "odf/package.h"

namespace docsmith::odf
{
namespace
{
using namespace std::chrono;

constexpr const char *base_meta = R"~~(<?xml version="1.0" encoding="UTF-8"?>
<office:document-meta
    xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    office:version="1.2">
    <office:meta/>
</office:document-meta>
)~~";

/// An xsd:dateTime ("2025-06-16T13:50:22.567", optionally ending in Z or
/// +hh:mm), to the second.
std::optional<sys_seconds> parse_date(std::string_view s)
{
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
    const std::string str(s);
    if(std::sscanf(str.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) < 3)
        return std::nullopt;
    const year_month_day ymd{year{y}, month(static_cast<unsigned>(mo)), day(static_cast<unsigned>(d))};
    if(!ymd.ok())
        return std::nullopt;
    sys_seconds t = sys_days{ymd} + hours{h} + minutes{mi} + seconds{sec};

    // The zone follows the time (and any fraction of a second).
    if(const auto tpos = s.find('T'); tpos != std::string_view::npos)
        if(const auto zpos = s.find_first_of("+-", tpos); zpos != std::string_view::npos)
        {
            int zh = 0, zm = 0;
            if(std::sscanf(str.c_str() + zpos + 1, "%d:%d", &zh, &zm) >= 1)
                t -= (s[zpos] == '-' ? -1 : 1) * (hours{zh} + minutes{zm});
        }
    return t;
}

std::string format_date(sys_seconds t)
{
    const auto days = floor<std::chrono::days>(t);
    const year_month_day ymd{days};
    const hh_mm_ss hms{t - days};
    using detail::zero_pad;
    return zero_pad(static_cast<int>(ymd.year()), 4) + "-" +
        zero_pad(static_cast<unsigned>(ymd.month()), 2) + "-" +
        zero_pad(static_cast<unsigned>(ymd.day()), 2) + "T" + zero_pad(hms.hours().count(), 2) +
        ":" + zero_pad(hms.minutes().count(), 2) + ":" + zero_pad(hms.seconds().count(), 2) + "Z";
}

/// Sets the one child element name to value, or removes it if value is empty.
void set_text(pugi::xml_node parent, const char *name, const std::string &value)
{
    auto n = parent.child(name);
    if(value.empty())
    {
        while(parent.remove_child(name))
            ;
        return;
    }
    if(!n)
        n = parent.append_child(name);
    if(value != n.text().get())
        n.text().set(value.c_str());
}

void set_date(pugi::xml_node parent, const char *name, const std::optional<sys_seconds> &value)
{
    // An unchanged date keeps its original form, fraction of a second included.
    if(auto n = parent.child(name); n && value && parse_date(n.text().get()) == value)
        return;
    set_text(parent, name, value ? format_date(*value) : std::string());
}

void set_keywords(pugi::xml_node parent, const std::vector<std::string> &keywords)
{
    std::vector<std::string> current;
    for(auto k : parent.children("meta:keyword"))
        current.emplace_back(k.text().get());
    if(current == keywords)
        return;
    while(parent.remove_child("meta:keyword"))
        ;
    for(const auto &k : keywords)
        parent.append_child("meta:keyword").text().set(k.c_str());
}

void set_custom(pugi::xml_node parent, const std::map<std::string, std::string> &custom)
{
    auto remaining = custom;
    for(auto n = parent.child("meta:user-defined"); n;)
    {
        const auto next = n.next_sibling("meta:user-defined");
        const auto it = remaining.find(n.attribute("meta:name").as_string());
        if(it == remaining.end())
            parent.remove_child(n); // dropped, or a repeated name
        else
        {
            if(it->second != n.text().get())
            {
                n.remove_attribute("meta:value-type"); // a changed value is a string
                n.text().set(it->second.c_str());
            }
            remaining.erase(it);
        }
        n = next;
    }
    for(const auto &[name, value] : remaining)
    {
        auto n = parent.append_child("meta:user-defined");
        n.append_attribute("meta:name") = name.c_str();
        n.text().set(value.c_str());
    }
}

void declare(pugi::xml_node root, const char *prefix, const char *uri)
{
    if(!root.attribute(prefix))
        root.append_attribute(prefix) = uri;
}

} // namespace

metadata read_meta(const std::string &xml)
{
    metadata m;
    pugi::xml_document doc;
    if(!doc.load_buffer(xml.data(), xml.size()))
        return m;
    const auto meta = doc.child("office:document-meta").child("office:meta");

    m.title = meta.child("dc:title").text().get();
    m.subject = meta.child("dc:subject").text().get();
    m.description = meta.child("dc:description").text().get();
    m.language = meta.child("dc:language").text().get();
    // initial-creator is the author; dc:creator is whoever saved it last.
    m.author = meta.child("meta:initial-creator").text().get();
    if(m.author.empty())
        m.author = meta.child("dc:creator").text().get();
    for(auto k : meta.children("meta:keyword"))
        m.keywords.emplace_back(k.text().get());
    m.created = parse_date(meta.child("meta:creation-date").text().get());
    m.modified = parse_date(meta.child("dc:date").text().get());
    for(auto u : meta.children("meta:user-defined"))
        m.custom.emplace(u.attribute("meta:name").as_string(), u.text().get());
    return m;
}

void write_meta(const metadata &m, package &pkg)
{
    pugi::xml_document doc;
    const auto *original = pkg.find("meta.xml");
    if(!original || !doc.load_buffer(original->data.data(), original->data.size()))
    {
        if(m == metadata{})
            return;
        doc.load_string(base_meta);
        doc.child("office:document-meta").attribute("office:version").set_value(pkg.version().c_str());
    }

    auto root = doc.child("office:document-meta");
    declare(root, "xmlns:meta", "urn:oasis:names:tc:opendocument:xmlns:meta:1.0");
    declare(root, "xmlns:dc", "http://purl.org/dc/elements/1.1/");
    auto meta = root.child("office:meta");
    if(!meta)
        meta = root.append_child("office:meta");

    set_text(meta, "dc:title", m.title);
    set_text(meta, "dc:subject", m.subject);
    set_text(meta, "dc:description", m.description);
    set_text(meta, "dc:language", m.language);
    // An author read from dc:creator (there was no initial-creator) stays there.
    if(meta.child("meta:initial-creator") || m.author != meta.child("dc:creator").text().get())
        set_text(meta, "meta:initial-creator", m.author);
    set_keywords(meta, m.keywords);
    set_date(meta, "meta:creation-date", m.created);
    set_date(meta, "dc:date", m.modified);
    set_custom(meta, m.custom);

    std::ostringstream ss;
    doc.save(ss, "", pugi::format_raw);
    pkg.set("meta.xml", ss.str(), "text/xml");
}

} // namespace docsmith::odf
