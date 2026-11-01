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
#include "docsmithcpp/text_doc.h"

#include <fstream>
#include <set>
#include <stdexcept>
#include <utility>

namespace docsmith
{
namespace fs = std::filesystem;

namespace
{
template <typename Registry, typename T>
void check_no_clash(const Registry &to, const std::string &name, const T &value, const char *what)
{
    if(const auto *existing = to.find(name); existing && !(*existing == value))
        throw std::invalid_argument(
            std::string(what) + " \"" + name + "\" is already defined differently");
}

/// Throws if from defines a name that to defines differently.
template <typename Registry>
void check_no_clash(const Registry &to, const Registry &from, const char *what)
{
    for(const auto &[name, value] : from)
        check_no_clash(to, name, value, what);
}

template <typename Registry>
void add_all(Registry &to, const Registry &from)
{
    for(const auto &[name, value] : from)
        if(!to.find(name))
            to.add(value);
}

template <typename Registry, typename T>
style_name add_checked(Registry &to, const T &value, const char *what)
{
    check_no_clash(to, value.m_name.get_name(), value, what);
    to.add(value);
    return value.m_name;
}
} // namespace

text_doc &text_doc::append(const text_doc &other)
{
    if(&other == this)
        return append(text_doc(other));

    check_no_clash(m_styles, other.m_styles, "Style");
    check_no_clash(m_list_styles, other.m_list_styles, "List style");
    check_no_clash(m_resources, other.m_resources, "Resource");

    add_all(m_styles, other.m_styles);
    add_all(m_list_styles, other.m_list_styles);
    add_all(m_resources, other.m_resources);
    body.insert(body.end(), other.body.begin(), other.body.end());
    return *this;
}

style_name text_doc::add_style(const style &s) { return add_checked(m_styles, s, "Style"); }

style_name text_doc::add_style(const list_style &ls)
{ return add_checked(m_list_styles, ls, "List style"); }

image text_doc::add_image(std::string name, std::string data, std::string media_type)
{
    image img{name};
    m_resources.add(resource(std::move(name), std::move(data), std::move(media_type)));
    return img;
}

paragraph *text_doc::find_title()
{
    return const_cast<paragraph *>(std::as_const(*this).find_title());
}

const paragraph *text_doc::find_title() const
{
    const auto titles =
        find_all<paragraph>(*this, [](const paragraph &p) { return p.style == builtin::title; });
    return titles.empty() ? nullptr : titles.front();
}

text_doc &text_doc::set_title(std::string text)
{
    paragraph p = paragraph(std::move(text)).set_style(builtin::title);
    if(paragraph *existing = find_title())
        *existing = std::move(p);
    else
        body.insert(body.begin(), block_node{std::move(p)});
    return *this;
}

void text_doc::write_resources(const fs::path &dir) const
{
    std::set<std::string> written;
    for(const frame *f : collect<frame>(*this))
    {
        const resource *r = m_resources.find(f->img.uri);
        if(!r || !written.insert(r->name()).second)
            continue;

        // Names come from loaded files too: keep them inside dir.
        const fs::path rel = fs::path(r->name()).lexically_normal();
        if(rel.empty() || rel.is_absolute() || rel.has_root_path() || *rel.begin() == "..")
            throw std::runtime_error("Resource name leaves the target folder: " + r->name());

        const fs::path p = dir / rel;
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        if(!out.write(r->data().data(), static_cast<std::streamsize>(r->data().size())))
            throw std::runtime_error("Could not write " + p.string());
    }
}

} // namespace docsmith
