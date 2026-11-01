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
#include "xml/namespaces.h"

#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

namespace docsmith::xml
{
namespace
{
class normalizer
{
public:
    explicit normalizer(std::span<const ns> known) :
        m_known(known)
    {
        for(const auto &k : known)
            m_known_prefixes.insert(k.prefix);
    }

    void run(pugi::xml_document &doc)
    {
        auto root = doc.document_element();
        if(!root)
            return;
        walk(root);

        // All declarations on the root: the known namespaces first, in order.
        std::vector<std::pair<std::string, std::string>> decls; // prefix, uri
        for(const auto &k : m_known)
            if(m_declared.contains(k.uri))
                decls.emplace_back(k.prefix, k.uri);
        for(const auto &[uri, prefix] : m_foreign)
            decls.emplace_back(prefix, uri);
        for(auto it = decls.rbegin(); it != decls.rend(); ++it)
            root.prepend_attribute(("xmlns:" + it->first).c_str()) = it->second.c_str();
    }

private:
    const char *lookup(std::string_view prefix) const
    {
        for(auto it = m_scope.rbegin(); it != m_scope.rend(); ++it)
            if(it->first == prefix)
                return it->second.c_str();
        return nullptr;
    }

    /// The prefix uri gets in the output; file_prefix is what the file used.
    const std::string &final_prefix(const std::string &uri, std::string_view file_prefix)
    {
        if(auto it = m_final.find(uri); it != m_final.end())
            return it->second;
        std::string p;
        for(const auto &k : m_known)
            if(uri == k.uri)
                p = k.prefix;
        if(p.empty())
        {
            p = std::string(file_prefix);
            for(int i = 1; p.empty() || m_known_prefixes.contains(p) || m_taken.contains(p); ++i)
                p = "ns" + std::to_string(i);
            m_taken.insert(p);
            m_foreign[uri] = p;
        }
        m_declared.insert(uri);
        return m_final[uri] = p;
    }

    /// name with its prefix renamed, or nullopt if its prefix isn't bound.
    std::optional<std::string> rename(std::string_view name, bool is_attribute)
    {
        const auto colon = name.find(':');
        const std::string_view prefix =
            colon == std::string_view::npos ? "" : name.substr(0, colon);
        const std::string_view local =
            colon == std::string_view::npos ? name : name.substr(colon + 1);
        if(prefix == "xml" || (is_attribute && prefix.empty()))
            return std::nullopt;
        const char *uri = lookup(prefix);
        if(!uri || !*uri)
            return std::nullopt;
        const std::string &p = final_prefix(uri, prefix);
        return p + ":" + std::string(local);
    }

    /// A list of prefixes (or prefixed names, with qnames) with each renamed.
    std::string rename_list(std::string_view value, bool qnames)
    {
        std::string out;
        std::istringstream in{std::string(value)};
        for(std::string token; in >> token;)
        {
            std::string renamed = token;
            if(qnames)
            {
                if(token.find(':') != std::string::npos)
                    if(auto r = rename(token, false))
                        renamed = *r;
            }
            else if(const char *uri = lookup(token); uri && *uri)
                renamed = final_prefix(uri, token);
            out += (out.empty() ? "" : " ") + renamed;
        }
        return out;
    }

    void walk(pugi::xml_node node)
    {
        const std::size_t mark = m_scope.size();
        for(auto a = node.first_attribute(); a;)
        {
            const auto next = a.next_attribute();
            const std::string_view name = a.name();
            if(name == "xmlns" || name.starts_with("xmlns:"))
            {
                const std::string prefix(name == "xmlns" ? "" : name.substr(6));
                m_scope.emplace_back(prefix, a.value());
                if(*a.value())
                    final_prefix(a.value(), prefix);
                node.remove_attribute(a);
            }
            a = next;
        }

        if(auto r = rename(node.name(), false))
            node.set_name(r->c_str());
        const bool choice = std::string_view(node.name()) == "mc:Choice";
        for(auto a : node.attributes())
        {
            if(auto r = rename(a.name(), true))
                a.set_name(r->c_str());
            const std::string_view name = a.name();
            if(name == "mc:Ignorable" || name == "mc:MustUnderstand" ||
                (choice && name == "Requires"))
                a.set_value(rename_list(a.value(), false).c_str());
            else if(name == "mc:ProcessContent")
                a.set_value(rename_list(a.value(), true).c_str());
        }

        for(auto c : node.children())
            if(c.type() == pugi::node_element)
                walk(c);
        m_scope.resize(mark);
    }

    std::span<const ns> m_known;
    std::set<std::string> m_known_prefixes;
    std::vector<std::pair<std::string, std::string>> m_scope; //!< prefix, uri
    std::map<std::string, std::string> m_final;               //!< uri -> output prefix
    std::map<std::string, std::string> m_foreign;             //!< uri -> prefix, not in known
    std::set<std::string> m_taken;                            //!< foreign prefixes
    std::set<std::string> m_declared;                         //!< uris to declare on the root
};
} // namespace

void normalize(pugi::xml_document &doc, std::span<const ns> known) { normalizer(known).run(doc); }

bool load(pugi::xml_document &doc, const std::string &data, std::span<const ns> known)
{
    if(!doc.load_buffer(data.data(), data.size(), pugi::parse_default | pugi::parse_ws_pcdata))
        return false;
    normalize(doc, known);
    return true;
}

std::string save(const pugi::xml_document &doc)
{
    std::ostringstream ss;
    doc.save(ss, "", pugi::format_raw);
    return ss.str();
}

std::string serialize(const pugi::xml_node &n)
{
    std::ostringstream ss;
    n.print(ss, "", pugi::format_raw);
    return ss.str();
}

void declare(pugi::xml_node root, std::span<const ns> namespaces)
{
    for(const auto &n : namespaces)
    {
        const std::string attr = std::string("xmlns:") + n.prefix;
        if(!root.attribute(attr.c_str()))
            root.append_attribute(attr.c_str()) = n.uri;
    }
}

} // namespace docsmith::xml
