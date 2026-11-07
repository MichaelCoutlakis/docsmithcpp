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
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

#include "outline.h"

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

bool has_bookmark(const std::vector<inline_node> &kids, const bookmark &b)
{
    for(const auto &k : kids)
        if(std::visit(
               overloaded{
                   [&](const bookmark &x) { return x == b; },
                   [&](const span &s) { return has_bookmark(s.children(), b); },
                   [&](const hyperlink &h) { return has_bookmark(h.children(), b); },
                   [](const auto &) { return false; },
               },
               k.v))
            return true;
    return false;
}

bool matches(const placeholder &p, const std::vector<inline_node> &kids, const style_name &style)
{
    return std::visit(
        overloaded{
            [&](const bookmark &b) { return has_bookmark(kids, b); },
            [&](const style_name &sn) { return style == sn; },
            [&](const text &t)
            {
                const std::string s = detail::plain_text(kids);
                const auto first = s.find_first_not_of(" \t\n");
                if(first == std::string::npos)
                    return t.content.empty();
                return s.substr(first, s.find_last_not_of(" \t\n") - first + 1) == t.content;
            },
        },
        p);
}

struct found_block
{
    std::vector<block_node> *blocks{nullptr};
    std::size_t index{0};
};

/// The first block in blocks, or in the lists and tables in them, that is the
/// placeholder.
found_block find_placeholder(std::vector<block_node> &blocks, const placeholder &p)
{
    for(std::size_t i = 0; i != blocks.size(); ++i)
    {
        found_block f = std::visit(
            overloaded{
                [&](paragraph &n)
                {
                    return matches(p, n.children(), n.style) ? found_block{&blocks, i}
                                                             : found_block{};
                },
                [&](heading &n)
                {
                    return matches(p, n.children(), n.style) ? found_block{&blocks, i}
                                                             : found_block{};
                },
                [&](list &l)
                {
                    for(auto &it : l.items())
                        if(auto f = find_placeholder(it.children(), p); f.blocks)
                            return f;
                    return found_block{};
                },
                [&](table &t)
                {
                    for(auto &c : t.cells())
                        if(auto f = find_placeholder(c.children(), p); f.blocks)
                            return f;
                    return found_block{};
                },
                [](auto &) { return found_block{}; },
            },
            blocks[i].v);
        if(f.blocks)
            return f;
    }
    return {};
}

template <typename Registry, typename T>
style_name add_if_new(Registry &to, const T &value)
{
    if(!to.find(value.m_name.get_name()))
        to.add(value);
    return value.m_name;
}

/// The family of a builtin style, if it is one.
std::optional<style_family> builtin_family(const style_name &name)
{
    if(name == builtin::source_text || name == builtin::link)
        return style_family::text;
    if(name == builtin::default_table)
        return style_family::table;
    if(name == builtin::default_paragraph || name == builtin::title ||
        name == builtin::preformatted)
        return style_family::paragraph;
    for(int level = 1; level <= 10; ++level)
        if(name == builtin::heading(level))
            return style_family::paragraph;
    return std::nullopt;
}
} // namespace

text_doc &text_doc::append(const text_doc &other)
{
    if(&other == this)
        return append(text_doc(other));

    merge_registries(other);
    body.insert(body.end(), other.body.begin(), other.body.end());
    return *this;
}

void text_doc::merge_registries(const text_doc &other)
{
    check_no_clash(m_styles, other.m_styles, "Style");
    check_no_clash(m_list_styles, other.m_list_styles, "List style");
    check_no_clash(m_resources, other.m_resources, "Resource");

    add_all(m_styles, other.m_styles);
    add_all(m_list_styles, other.m_list_styles);
    add_all(m_resources, other.m_resources);
}

bool text_doc::replace(const placeholder &p, const text_doc &content)
{ return fill(p, content, fill_mode::replace); }

bool text_doc::insert_after(const placeholder &p, const text_doc &content)
{ return fill(p, content, fill_mode::after); }

bool text_doc::insert_before(const placeholder &p, const text_doc &content)
{ return fill(p, content, fill_mode::before); }

bool text_doc::fill(const placeholder &p, const text_doc &content, fill_mode mode)
{
    if(&content == this)
        return fill(p, text_doc(content), mode);

    found_block f;
    for(auto *blocks : {&body, &page.header, &page.footer})
        if(!f.blocks)
            f = find_placeholder(*blocks, p);
    if(!f.blocks)
        return false;

    merge_registries(content);
    auto pos = f.blocks->begin() + static_cast<std::ptrdiff_t>(f.index);
    if(mode == fill_mode::replace)
        pos = f.blocks->erase(pos);
    else if(mode == fill_mode::after)
        ++pos;
    f.blocks->insert(pos, content.body.begin(), content.body.end());
    return true;
}

style_name text_doc::add_style(const style &s) { return add_if_new(m_styles, s); }

style_name text_doc::add_style(const list_style &ls) { return add_if_new(m_list_styles, ls); }

style_name text_doc::add_or_overwrite_style(const style &s) { return m_styles.add(s).m_name; }

style_name text_doc::add_or_overwrite_style(const list_style &ls)
{ return m_list_styles.add(ls).m_name; }

style &text_doc::builtin_style(const style_name &name)
{
    if(name == builtin::default_list)
        throw std::invalid_argument("builtin::default_list is a list style");
    if(style *s = m_styles.find(name.get_name()))
        return *s;
    style s(name);
    if(auto f = builtin_family(name))
        s.set(*f);
    return m_styles.add(s);
}

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
