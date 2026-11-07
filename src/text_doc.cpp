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
#include "docsmithcpp/error.h"

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
/// Throws if from defines a name that to defines differently.
template <typename Registry>
void check_no_clash(const Registry &to, const Registry &from, const char *what)
{
    for(const auto &[name, value] : from)
        if(const auto *existing = to.find(name); existing && !(*existing == value))
        {
            std::string shown;
            // On the key type, not the binding: Clang before 16 can't name a
            // structured binding in a requires expression.
            if constexpr(requires(const typename Registry::key_type &k) { k.str(); })
                shown = name.str();
            else
                shown = name;
            throw std::invalid_argument(
                std::string(what) + " \"" + shown + "\" is already defined differently");
        }
}

template <typename Registry>
void add_all(Registry &to, const Registry &from)
{
    for(const auto &[name, value] : from)
        to.insert(value);
}

bool has_bookmark(const std::vector<inline_node> &kids, const bookmark &b)
{
    for(const auto &k : kids)
        if(std::visit(
               overloaded{
                   [&](const bookmark &x) { return x == b; },
                   [&](const span &s) { return has_bookmark(s.items(), b); },
                   [&](const hyperlink &h) { return has_bookmark(h.items(), b); },
                   [](const auto &) { return false; },
               },
               k.v))
            return true;
    return false;
}

bool matches(
    const placeholder &p, const std::vector<inline_node> &kids, const para_style_name &style)
{
    return std::visit(
        overloaded{
            [&](const bookmark &b) { return has_bookmark(kids, b); },
            [&](const para_style_name &sn) { return style == sn; },
            [&](const text &t)
            {
                const std::string s = detail::plain_text(kids);
                const auto first = s.find_first_not_of(" \t\n");
                if(first == std::string::npos)
                    return t.content.empty();
                return s.substr(first, s.find_last_not_of(" \t\n") - first + 1) == t.content;
            },
        },
        p.v);
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
                    return matches(p, n.items(), n.style) ? found_block{&blocks, i} : found_block{};
                },
                [&](heading &n)
                {
                    return matches(p, n.items(), n.style) ? found_block{&blocks, i} : found_block{};
                },
                [&](list &l)
                {
                    for(auto &it : l.items())
                        if(auto f = find_placeholder(it.items(), p); f.blocks)
                            return f;
                    return found_block{};
                },
                [&](table &t)
                {
                    for(auto &c : t.cells())
                        if(auto f = find_placeholder(c.items(), p); f.blocks)
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

} // namespace

text_doc &text_doc::append(const text_doc &other) &
{
    if(&other == this)
        return append(text_doc(other));

    merge_registries(other);
    body.insert(body.end(), other.body.begin(), other.body.end());
    return *this;
}

void text_doc::merge_registries(const text_doc &other)
{
    // Each family's registry of this document and other's.
    auto each_family = [&](auto &&f)
    {
        f(m_styles.para, other.m_styles.para);
        f(m_styles.text, other.m_styles.text);
        f(m_styles.table, other.m_styles.table);
        f(m_styles.cell, other.m_styles.cell);
        f(m_styles.row, other.m_styles.row);
        f(m_styles.frame, other.m_styles.frame);
        f(m_styles.list, other.m_styles.list);
    };
    // All checks first, so a clash changes nothing.
    each_family(
        [](const auto &mine, const auto &theirs) { check_no_clash(mine, theirs, "Style"); });
    check_no_clash(m_resources, other.m_resources, "Resource");

    each_family([](auto &mine, const auto &theirs) { add_all(mine, theirs); });
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

image text_doc::add_image(std::string name, std::string data, std::string media_type)
{
    image img{name};
    m_resources.insert_or_assign(resource(std::move(name), std::move(data), std::move(media_type)));
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

text_doc &text_doc::set_title(std::string text) &
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
            throw error(
                error_code::unsafe_path, "Resource name leaves the target folder: " + r->name());

        const fs::path p = dir / rel;
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        if(!out.write(r->data().data(), static_cast<std::streamsize>(r->data().size())))
            throw error(error_code::io, "Could not write " + p.string());
    }
}

} // namespace docsmith
