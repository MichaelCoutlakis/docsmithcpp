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
#include "odt/writer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <variant>

#include "format.h"
#include "odf/package.h"
#include "odt/styles.h"

namespace docsmith::odt
{
namespace fs = std::filesystem;

namespace
{
std::string media_type_of(const fs::path &p)
{
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if(ext == ".png")
        return "image/png";
    if(ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if(ext == ".gif")
        return "image/gif";
    if(ext == ".svg")
        return "image/svg+xml";
    if(ext == ".bmp")
        return "image/bmp";
    return {};
}

std::string read_file(const fs::path &p)
{
    std::ifstream in(p, std::ios::binary);
    if(!in)
        throw std::runtime_error("Could not read file: " + p.string());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

constexpr const char *base_content = R"~~(<?xml version="1.0" encoding="UTF-8"?>
<office:document-content
    xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0"
    xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"
    xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
    xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
    xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0"
    xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:loext="urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0"
    office:version="1.2">
    <office:font-face-decls/>
    <office:automatic-styles/>
    <office:body/>
</office:document-content>
)~~";

constexpr const char *base_styles = R"~~(<?xml version="1.0" encoding="UTF-8"?>
<office:document-styles
    xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0"
    xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"
    xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
    xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
    xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0"
    xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:loext="urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0"
    office:version="1.2">
    <office:font-face-decls/>
    <office:styles/>
    <office:automatic-styles/>
    <office:master-styles/>
</office:document-styles>
)~~";

// LibreOffice's Title, its Heading parent's properties included.
constexpr const char *title_style = R"~~(<style:style style:name="Title" style:family="paragraph" style:class="chapter">
    <style:paragraph-properties fo:margin-top="0.423cm" fo:margin-bottom="0.212cm" fo:keep-with-next="always" fo:text-align="center"/>
    <style:text-properties fo:font-family="'Liberation Sans'" style:font-family-generic="swiss" fo:font-size="28pt" fo:font-weight="bold" style:font-size-asian="28pt" style:font-weight-asian="bold" style:font-size-complex="28pt" style:font-weight-complex="bold"/>
</style:style>)~~";

/// The families each style name is used with in the body.
std::map<std::string, std::set<style_family>> used_families(const text_doc &doc)
{
    std::map<std::string, std::set<style_family>> out;
    walk(doc,
        [&out](const auto &n)
        {
            using T = std::decay_t<decltype(n)>;
            auto use = [&](const style_name &sn, style_family f)
            {
                if(!sn.is_empty())
                    out[sn.get_name()].insert(f);
            };
            if constexpr(std::is_same_v<T, paragraph> || std::is_same_v<T, heading>)
                use(n.style, style_family::paragraph);
            else if constexpr(std::is_same_v<T, span>)
                use(n.style, style_family::text);
            else if constexpr(std::is_same_v<T, frame>)
                use(n.style, style_family::graphic);
            else if constexpr(std::is_same_v<T, table_cell>)
                use(n.style, style_family::table_cell);
        });
    return out;
}

/// The families to write s in: its own, else those it's used with, else
/// paragraph.
std::set<style_family> families_of(
    const style &s, const std::map<std::string, std::set<style_family>> &used)
{
    if(s.m_family)
        return {*s.m_family};
    if(auto it = used.find(s.m_name.get_name()); it != used.end())
        return it->second;
    return {style_family::paragraph};
}

/// Declares on dst the namespaces src declares and dst doesn't.
void add_namespaces(pugi::xml_node dst, const pugi::xml_node &src)
{
    for(auto a : src.attributes())
        if(std::string_view(a.name()).starts_with("xmlns:") && !dst.attribute(a.name()))
            dst.append_attribute(a.name()) = a.value();
}

/// The style:style in container with that name and family.
pugi::xml_node find_style(const pugi::xml_node &container, const std::string &name, style_family f)
{
    for(auto c : container.children("style:style"))
        if(name == c.attribute("style:name").as_string())
            if(auto s = read_style(c); s && s->m_family == f)
                return c;
    return {};
}

pugi::xml_node find_list_style(const pugi::xml_node &container, const std::string &name)
{ return container.find_child_by_attribute("text:list-style", "style:name", name.c_str()); }

/// Writes def in place of existing, or appends it to fallback if there's no
/// existing definition. An unchanged definition is left as it was, so the
/// properties the model doesn't hold survive.
template <typename Def>
void replace_or_add(
    pugi::xml_node existing, pugi::xml_node fallback, const Def &def, bool unchanged)
{
    if(existing && unchanged)
        return;
    pugi::xml_node parent = existing ? existing.parent() : fallback;
    write_xml(parent, def);
    if(existing)
    {
        parent.insert_move_before(parent.last_child(), existing);
        parent.remove_child(existing);
    }
}

/// A registry's definitions in name order, so the output is stable.
template <typename Registry>
auto sorted(const Registry &r)
{
    std::vector<const typename Registry::mapped_type *> out;
    for(const auto &[name, def] : r)
        out.push_back(&def);
    std::sort(out.begin(),
        out.end(),
        [](const auto *a, const auto *b) { return a->m_name.get_name() < b->m_name.get_name(); });
    return out;
}
} // namespace

void write_document(const text_doc &doc, odf::package &pkg) { writer(pkg).write(doc); }

writer::writer(odf::package &pkg) :
    m_pkg(pkg)
{
    m_content.load_string(base_content);
    auto root = m_content.child("office:document-content");
    root.attribute("office:version").set_value(m_pkg.version().c_str());
    m_font_faces = root.child("office:font-face-decls");
    m_automatic_styles = root.child("office:automatic-styles");
    m_body = root.child("office:body");
    carry_over();
    load_styles_xml();
}

void writer::write(const text_doc &doc)
{
    m_doc = &doc;
    write_styles(doc);
    write_doc(doc);

    auto save = [](const pugi::xml_document &xml)
    {
        std::ostringstream ss;
        xml.save(ss, "  ");
        return ss.str();
    };
    m_pkg.set("content.xml", save(m_content), "text/xml");
    m_pkg.set("styles.xml", save(m_styles_xml), "text/xml");
}

// A loaded document's content.xml declares things the model doesn't hold: the
// namespaces its markup uses, its fonts, and its automatic styles (P1, T1, ...).
// Carry them over so the style names on parsed nodes, and any unknown nodes,
// still resolve after saving.
void writer::carry_over()
{
    const auto *original = m_pkg.find("content.xml");
    pugi::xml_document xml;
    if(!original || !xml.load_buffer(original->data.data(), original->data.size()))
        return;

    auto src = xml.child("office:document-content");
    add_namespaces(m_content.child("office:document-content"), src);

    for(auto c : src.child("office:font-face-decls").children())
        m_font_faces.append_copy(c);
    for(auto c : src.child("office:automatic-styles").children())
        m_automatic_styles.append_copy(c);
}

// The writer owns styles.xml. A loaded one is kept and updated in place, so its
// default styles, page layouts and master pages (headers, footers) survive.
void writer::load_styles_xml()
{
    pugi::xml_document base;
    base.load_string(base_styles);
    auto base_root = base.child("office:document-styles");

    const auto *original = m_pkg.find("styles.xml");
    if(original && m_styles_xml.load_buffer(original->data.data(), original->data.size()) &&
        m_styles_xml.child("office:document-styles"))
    {
        auto root = m_styles_xml.child("office:document-styles");
        add_namespaces(root, base_root);
        m_styles = root.child("office:styles");
        if(!m_styles)
        {
            auto fonts = root.child("office:font-face-decls");
            m_styles = fonts ? root.insert_child_after("office:styles", fonts)
                             : root.prepend_child("office:styles");
        }
        return;
    }

    m_styles_xml.reset(base);
    auto root = m_styles_xml.child("office:document-styles");
    root.attribute("office:version").set_value(m_pkg.version().c_str());
    m_styles = root.child("office:styles");
}

// Each definition in the registries is written where the loaded file had it,
// or, if it's new, common styles to styles.xml and list styles to content.xml's
// automatic styles. One that is unchanged since loading keeps its original XML.
void writer::write_styles(const text_doc &doc)
{
    const auto used = used_families(doc);
    for(const style *s : sorted(doc.styles()))
        for(style_family f : families_of(*s, used))
        {
            style def = *s;
            def.m_family = f;
            auto existing = find_style(m_automatic_styles, def.m_name.get_name(), f);
            if(!existing)
                existing = find_style(m_styles, def.m_name.get_name(), f);
            replace_or_add(existing, m_styles, def, existing && read_style(existing) == def);
        }

    for(const list_style *ls : sorted(doc.list_styles()))
    {
        auto existing = find_list_style(m_automatic_styles, ls->m_name.get_name());
        if(!existing)
            existing = find_list_style(m_styles, ls->m_name.get_name());
        replace_or_add(
            existing, m_automatic_styles, *ls, existing && read_list_style(existing) == *ls);
    }

    // LibreOffice knows Title without a definition; other readers may not.
    if(doc.find_title() &&
        !find_style(m_styles, builtin::title.get_name(), style_family::paragraph))
        m_styles.append_buffer(title_style, std::strlen(title_style));
}

// Archive path for an image. A document resource is embedded under its name; an
// entry the package already holds is referenced in place; a file on disk is
// copied in under Pictures/, renamed if a different image already has that name.
std::string writer::resolve_image(const std::string &uri)
{
    if(const resource *r = m_doc->resources().find(uri))
    {
        if(const auto *e = m_pkg.find(uri); !e || e->data != r->data())
            m_pkg.set(uri, r->data(), r->media_type());
        return uri;
    }
    if(m_pkg.contains(uri))
        return uri;
    if(!fs::is_regular_file(uri))
        throw std::runtime_error("Image not found in the package or on disk: " + uri);

    std::string data = read_file(uri);
    const fs::path name = fs::path(uri).filename();
    std::string href = ("Pictures" / name).generic_string();
    for(int i = 2; m_pkg.contains(href) && m_pkg.find(href)->data != data; ++i)
        href = (fs::path("Pictures") /
            (name.stem().string() + "_" + std::to_string(i) + name.extension().string()))
                   .generic_string();

    if(!m_pkg.contains(href))
        m_pkg.set(href, std::move(data), media_type_of(uri));
    return href;
}

void writer::write_doc(const text_doc &doc)
{
    auto office_text = m_body.append_child("office:text");
    for(const auto &b : doc.body)
        write_block(office_text, b);
}

void writer::write_block(pugi::xml_node parent, const block_node &b)
{
    std::visit([&](const auto &n) { emit(parent, n); }, b.v);
}

void writer::write_inline(pugi::xml_node parent, const inline_node &i)
{
    std::visit([&](const auto &n) { emit(parent, n); }, i.v);
}

void writer::emit(pugi::xml_node parent, const paragraph &p)
{
    auto n = parent.append_child("text:p");
    if(!p.style.is_empty())
        n.append_attribute("text:style-name").set_value(p.style.get_name().c_str());
    for(const auto &c : p.children())
        write_inline(n, c);
}

void writer::emit(pugi::xml_node parent, const heading &h)
{
    auto n = parent.append_child("text:h");
    n.append_attribute("text:outline-level").set_value(h.level);
    if(!h.style.is_empty())
        n.append_attribute("text:style-name").set_value(h.style.get_name().c_str());
    for(const auto &c : h.children())
        write_inline(n, c);
}

void writer::emit(pugi::xml_node parent, const span &s)
{
    auto n = parent.append_child("text:span");
    if(!s.style.is_empty())
        n.append_attribute("text:style-name").set_value(s.style.get_name().c_str());
    for(const auto &c : s.children())
        write_inline(n, c);
}

void writer::emit(pugi::xml_node parent, const hyperlink &h)
{
    auto n = parent.append_child("text:a");
    n.append_attribute("xlink:type").set_value("simple");
    const std::string href = std::visit(overloaded{[](const url_target &u) { return u.url; },
                                            [](const bookmark_target &b) { return "#" + b.name; }},
        h.target);
    n.append_attribute("xlink:href").set_value(href.c_str());
    for(const auto &c : h.children())
        write_inline(n, c);
}

void writer::emit(pugi::xml_node parent, const text &t)
{
    parent.append_child(pugi::node_pcdata).set_value(t.content.c_str());
}

void writer::emit(pugi::xml_node parent, const frame &f)
{
    auto n = parent.append_child("draw:frame");
    if(!f.style.is_empty())
        n.append_attribute("draw:style-name").set_value(f.style.get_name().c_str());
    emit(n, f.img);
}

void writer::emit(pugi::xml_node parent, const image &v)
{
    const std::string href = resolve_image(v.uri);

    auto n = parent.append_child("draw:image");
    n.append_attribute("xlink:href").set_value(href.c_str());
    n.append_attribute("xlink:type").set_value("simple");
    n.append_attribute("xlink:show").set_value("embed");
    n.append_attribute("draw:mime-type").set_value(m_pkg.find(href)->media_type.c_str());
    n.append_attribute("xlink:actuate").set_value("onLoad");
}

void writer::emit(pugi::xml_node parent, const bookmark &b)
{
    auto n = parent.append_child("text:bookmark");
    n.append_attribute("text:name").set_value(b.name.c_str());
}

void writer::emit(pugi::xml_node parent, const unknown &u)
{
    // Re-emit the captured subtree verbatim.
    pugi::xml_document frag;
    if(frag.load_string(u.xml.c_str()))
        for(auto child : frag.children())
            parent.append_copy(child);
}

void writer::emit(pugi::xml_node parent, const list &l)
{
    auto n = parent.append_child("text:list");
    if(!l.style.is_empty())
        n.append_attribute("text:style-name").set_value(l.style.get_name().c_str());
    for(const auto &it : l.items())
        emit(n, it);
}

void writer::emit(pugi::xml_node parent, const list_item &it)
{
    auto n = parent.append_child("text:list-item");
    for(const auto &b : it.children())
        write_block(n, b);
}

void writer::emit(pugi::xml_node parent, const table &t)
{
    auto n = parent.append_child("table:table");
    auto table_column = n.append_child("table:table-column");
    table_column.append_attribute("table:number-columns-repeated")
        .set_value(static_cast<unsigned>(t.num_cols()));

    for(size_t r = 0; r != t.num_rows(); ++r)
    {
        auto xml_row = n.append_child("table:table-row");
        for(size_t c = 0; c != t.num_cols(); ++c)
        {
            if(t.is_cell_covered(r, c))
                xml_row.append_child("table:covered-table-cell");
            else
                emit(xml_row, t.cell(r, c));
        }
    }
}

void writer::emit(pugi::xml_node parent, const table_cell &c)
{
    auto n = parent.append_child("table:table-cell");
    if(!c.style.is_empty())
        n.append_attribute("table:style-name").set_value(c.style.get_name().c_str());
    n.append_attribute("office:value-type").set_value("string");

    if(c.rowspan != 1)
        n.append_attribute("table:number-rows-spanned").set_value(c.rowspan);
    if(c.colspan != 1)
        n.append_attribute("table:number-columns-spanned").set_value(c.colspan);

    for(const auto &b : c.children())
        write_block(n, b);
}

} // namespace docsmith::odt
