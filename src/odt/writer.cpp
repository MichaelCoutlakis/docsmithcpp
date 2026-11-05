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
#include "outline.h"

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
    xmlns:number="urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0"
    xmlns:loext="urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0"
    office:version="1.2">
    <office:font-face-decls/>
    <office:styles/>
    <office:automatic-styles/>
    <office:master-styles/>
</office:document-styles>
)~~";

// LibreOffice's Title, its Heading parent's properties included.
constexpr const char *title_style =
    R"~~(<style:style style:name="Title" style:family="paragraph" style:class="chapter"><style:paragraph-properties fo:margin-top="0.423cm" fo:margin-bottom="0.212cm" fo:keep-with-next="always" fo:text-align="center"/><style:text-properties fo:font-family="'Liberation Sans'" style:font-family-generic="swiss" fo:font-size="28pt" fo:font-weight="bold" style:font-size-asian="28pt" style:font-weight-asian="bold" style:font-size-complex="28pt" style:font-weight-complex="bold"/></style:style>)~~";

// LibreOffice's Preformatted Text and Source Text.
constexpr const char *preformatted_style =
    R"~~(<style:style style:name="Preformatted_20_Text" style:display-name="Preformatted Text" style:family="paragraph" style:class="html"><style:paragraph-properties fo:margin-top="0cm" fo:margin-bottom="0cm"/><style:text-properties fo:font-family="'Liberation Mono'" style:font-family-generic="modern" style:font-pitch="fixed" fo:font-size="10pt" style:font-size-asian="10pt" style:font-size-complex="10pt"/></style:style>)~~";
constexpr const char *source_text_style =
    R"~~(<style:style style:name="Source_20_Text" style:display-name="Source Text" style:family="text"><style:text-properties fo:font-family="'Liberation Mono'" style:font-family-generic="modern" style:font-pitch="fixed"/></style:style>)~~";

// YYYY-MM-DD, for date fields. A common style, so fields in content.xml and
// in styles.xml (headers, footers) can both use it.
constexpr const char *date_style_name = "docsmith_iso_date";
constexpr const char *date_style =
    R"~~(<number:date-style style:name="docsmith_iso_date"><number:year number:style="long"/><number:text>-</number:text><number:month number:style="long"/><number:text>-</number:text><number:day number:style="long"/></number:date-style>)~~";

// LibreOffice's Contents Heading and Contents 1-10: a right tab with dots for
// the page number, and each level indented by 0.5cm.
constexpr const char *contents_heading_style =
    R"~~(<style:style style:name="Contents_20_Heading" style:display-name="Contents Heading" style:family="paragraph" style:class="index"><style:paragraph-properties fo:margin-top="0.423cm" fo:margin-bottom="0.212cm" fo:keep-with-next="always"/><style:text-properties fo:font-size="16pt" fo:font-weight="bold" style:font-size-asian="16pt" style:font-weight-asian="bold" style:font-size-complex="16pt" style:font-weight-complex="bold"/></style:style>)~~";

std::string contents_style(int level)
{
    const std::string n = std::to_string(level);
    return R"~~(<style:style style:name="Contents_20_)~~" + n +
        R"~~(" style:display-name="Contents )~~" + n +
        R"~~(" style:family="paragraph" style:class="index"><style:paragraph-properties fo:margin-left=")~~" +
        detail::to_string(0.5f * static_cast<float>(level - 1)) +
        R"~~(cm" fo:margin-right="0cm" fo:text-indent="0cm"><style:tab-stops><style:tab-stop style:position=")~~" +
        detail::to_string(17 - 0.5f * static_cast<float>(level - 1)) +
        R"~~(cm" style:type="right" style:leader-style="dotted" style:leader-text="."/></style:tab-stops></style:paragraph-properties></style:style>)~~";
}

/// The builtin styles the writer defines when the document uses one and
/// neither it nor the loaded file defines it. LibreOffice knows them without a
/// definition; other readers may not.
struct builtin_def
{
    const style_name &name;
    style_family family;
    const char *xml;
};
const builtin_def builtin_defs[] = {
    {builtin::title, style_family::paragraph, title_style},
    {builtin::preformatted, style_family::paragraph, preformatted_style},
    {builtin::source_text, style_family::text, source_text_style},
};

/// The families each style name is used with in the body.
std::map<std::string, std::set<style_family>> used_families(const text_doc &doc)
{
    std::map<std::string, std::set<style_family>> out;
    walk(doc,
        [&out, &doc](const auto &n)
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
            else if constexpr(std::is_same_v<T, table>)
            {
                use(n.style, style_family::table);
                for(size_t r = 0; r != n.num_rows(); ++r)
                    use(n.row_style(r), style_family::table_row);
                for(size_t c = 0; c != n.num_cols(); ++c)
                    use(n.column_style(c), style_family::table_cell);
                for(size_t r = 0; r != n.num_rows(); ++r)
                    for(size_t c = 0; c != n.num_cols(); ++c)
                        if(!n.is_cell_covered(r, c))
                            use(effective_cell_props(n, doc.styles(), r, c).m_paragraph_style,
                                style_family::paragraph);
            }
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

/// Declares the font style:font-name refers to, unless decls has it.
void add_font_face(pugi::xml_node decls, const std::string &name)
{
    if(decls.find_child_by_attribute("style:font-face", "style:name", name.c_str()))
        return;
    auto f = decls.append_child("style:font-face");
    f.append_attribute("style:name") = name.c_str();
    f.append_attribute("svg:font-family") = ("'" + name + "'").c_str();
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
    write_page(doc.page);

    // Unindented: indentation between inline elements would be read as spaces.
    auto save = [](const pugi::xml_document &xml)
    {
        std::ostringstream ss;
        xml.save(ss, "", pugi::format_raw);
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

    // Both parts, as their styles may each use the font.
    auto root = m_styles_xml.child("office:document-styles");
    auto fonts = root.child("office:font-face-decls");
    if(!fonts)
        fonts = root.prepend_child("office:font-face-decls");
    for(const style *s : sorted(doc.styles()))
        if(s->m_text_props && s->m_text_props->m_font_name)
            for(auto decls : {fonts, m_font_faces})
                add_font_face(decls, s->m_text_props->m_font_name->get_name());

    for(const auto &b : builtin_defs)
        if(auto it = used.find(b.name.get_name()); it != used.end() &&
            it->second.contains(b.family) && !find_style(m_styles, b.name.get_name(), b.family))
            m_styles.append_buffer(b.xml, std::strlen(b.xml));
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
    write_blocks(office_text, doc.body);
}

// The header and footer go on the master page the body uses (see
// parse_page()). A new document only gets one if it has a header or footer:
// A4 with 2cm margins, LibreOffice's default.
void writer::write_page(const page_style &page)
{
    auto root = m_styles_xml.child("office:document-styles");
    auto masters = root.child("office:master-styles");
    auto master = masters.find_child_by_attribute("style:master-page", "style:name", "Standard");
    if(!master)
        master = masters.child("style:master-page");
    if(!master && page.header.empty() && page.footer.empty())
        return;

    if(!masters)
        masters = root.append_child("office:master-styles");
    auto automatic = root.child("office:automatic-styles");
    if(!automatic)
        automatic = root.insert_child_before("office:automatic-styles", masters);
    if(!master)
    {
        std::string name = "pm1";
        for(int i = 2;
            automatic.find_child_by_attribute("style:page-layout", "style:name", name.c_str());
            ++i)
            name = "pm" + std::to_string(i);
        auto layout = automatic.append_child("style:page-layout");
        layout.append_attribute("style:name") = name.c_str();
        auto props = layout.append_child("style:page-layout-properties");
        for(auto [attr, value] : {std::pair{"fo:page-width", "21.001cm"},
                {"fo:page-height", "29.7cm"},
                {"style:print-orientation", "portrait"},
                {"fo:margin-top", "2cm"},
                {"fo:margin-bottom", "2cm"},
                {"fo:margin-left", "2cm"},
                {"fo:margin-right", "2cm"}})
            props.append_attribute(attr) = value;
        master = masters.append_child("style:master-page");
        master.append_attribute("style:name") = "Standard";
        master.append_attribute("style:page-layout-name") = name.c_str();
    }

    // Without spacing in its page layout, a header touches the body.
    auto layout = automatic.find_child_by_attribute(
        "style:page-layout", "style:name", master.attribute("style:page-layout-name").as_string());
    auto spacing = [&](const char *style, const char *margin)
    {
        if(!layout)
            return;
        auto s = layout.child(style);
        if(!s)
            s = layout.append_child(style);
        if(!s.child("style:header-footer-properties"))
        {
            auto props = s.append_child("style:header-footer-properties");
            props.append_attribute("fo:min-height") = "0cm";
            props.append_attribute(margin) = "0.5cm";
        }
    };

    // The style:header / style:footer element is kept, with its attributes,
    // and its content replaced.
    auto write = [&](const char *tag, const std::vector<block_node> &blocks, pugi::xml_node after)
    {
        auto n = master.child(tag);
        if(blocks.empty())
        {
            master.remove_child(n);
            return;
        }
        if(!n)
            n = after ? master.insert_child_after(tag, after) : master.prepend_child(tag);
        while(n.first_child())
            n.remove_child(n.first_child());
        write_blocks(n, blocks);
    };
    write("style:header", page.header, {});
    if(!page.header.empty())
        spacing("style:header-style", "fo:margin-bottom");

    // ODF orders them header, header-left, header-first, then the footers.
    pugi::xml_node after;
    for(auto c : master.children())
        if(std::string_view(c.name()).starts_with("style:header"))
            after = c;
    write("style:footer", page.footer, after);
    if(!page.footer.empty())
        spacing("style:footer-style", "fo:margin-top");
}

void writer::write_blocks(pugi::xml_node parent, const std::vector<block_node> &blocks)
{
    for(const auto &b : blocks)
        write_block(parent, b);
    flush_page_break(parent); // a page break with no paragraph after it
}

// ODF has no page break element: a paragraph style's fo:break-before starts the
// page. So a page_break is folded into the next paragraph or heading, whose
// style is swapped for a copy (an automatic style) or a child (a common one) of
// it with the break added.
std::string writer::page_break_style(const std::string &name, const style_name &base)
{
    if(!m_page_break_styles.insert(name).second)
        return name;

    // A loaded file may have it already, made from base as it was then.
    if(auto old = find_style(m_automatic_styles, name, style_family::paragraph))
        m_automatic_styles.remove_child(old);

    pugi::xml_node s;
    auto automatic = base.is_empty()
        ? pugi::xml_node{}
        : find_style(m_automatic_styles, base.get_name(), style_family::paragraph);
    if(automatic)
    {
        s = m_automatic_styles.append_copy(automatic);
        s.attribute("style:name").set_value(name.c_str());
    }
    else
    {
        s = m_automatic_styles.append_child("style:style");
        s.append_attribute("style:name") = name.c_str();
        s.append_attribute("style:family") = "paragraph";
        if(!base.is_empty())
            s.append_attribute("style:parent-style-name") = base.get_name().c_str();
    }
    auto props = s.child("style:paragraph-properties");
    if(!props)
        props = s.prepend_child("style:paragraph-properties");
    if(!props.attribute("fo:break-before"))
        props.append_attribute("fo:break-before");
    props.attribute("fo:break-before").set_value("page");
    return name;
}

void writer::flush_page_break(pugi::xml_node parent)
{
    if(!m_page_break_pending)
        return;
    m_page_break_pending = false;
    parent.append_child("text:p").append_attribute("text:style-name") =
        page_break_style(page_break_only, {}).c_str();
}

/// The style name to write on a paragraph or heading, taking a pending page
/// break; null for none.
const char *writer::block_style(const style_name &sn)
{
    if(m_page_break_pending)
    {
        m_page_break_pending = false;
        m_block_style = page_break_style(page_break_style_name(sn), sn);
    }
    else
        m_block_style = sn.get_name();
    return m_block_style.empty() ? nullptr : m_block_style.c_str();
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
    if(const char *style = block_style(p.style.is_empty() ? m_cell_paragraph_style : p.style))
        n.append_attribute("text:style-name").set_value(style);
    for(const auto &c : p.children())
        write_inline(n, c);
}

void writer::emit(pugi::xml_node parent, const heading &h)
{
    auto n = parent.append_child("text:h");
    n.append_attribute("text:outline-level").set_value(h.level);
    if(const char *style = block_style(h.style))
        n.append_attribute("text:style-name").set_value(style);
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

// ODF collapses white space in text: a run of spaces reads as one, and a space
// at the start of a paragraph is dropped. So only a single space between two
// other characters is written as is; other spaces become text:s, and tabs and
// line breaks their own elements. '\r' is dropped, so "\r\n" is one break.
void writer::emit(pugi::xml_node parent, const text &t)
{
    const std::string &s = t.content;
    std::string run; // text not yet written
    auto flush = [&]
    {
        if(!run.empty())
            parent.append_child(pugi::node_pcdata).set_value(run.c_str());
        run.clear();
    };
    auto element = [&](const char *name)
    {
        flush();
        return parent.append_child(name);
    };

    for(size_t i = 0; i != s.size();)
    {
        const char c = s[i];
        if(c == ' ')
        {
            const size_t end = std::min(s.find_first_not_of(' ', i), s.size());
            size_t n = end - i;
            const bool between = !run.empty() && run.back() != ' ' && end != s.size() &&
                s[end] != '\n' && s[end] != '\t' && s[end] != '\r';
            if(between)
            {
                run += ' ';
                --n;
            }
            if(n != 0)
            {
                auto e = element("text:s");
                if(n > 1)
                    e.append_attribute("text:c") = static_cast<unsigned>(n);
            }
            i = end;
            continue;
        }
        if(c == '\n')
            element("text:line-break");
        else if(c == '\t')
            element("text:tab");
        else if(c != '\r')
            run += c;
        ++i;
    }
    flush();
}

// Frames are anchored as characters, in a line of text: the model doesn't
// hold other anchors or positions (DESIGN.md, Rules). A block frame is an
// unstyled paragraph of its own, as in DOCX.
void writer::emit(pugi::xml_node parent, const frame &f)
{
    const std::string_view in = parent.name();
    if(in != "text:p" && in != "text:h" && in != "text:span" && in != "text:a")
    {
        flush_page_break(parent);
        parent = parent.append_child("text:p");
    }
    auto n = parent.append_child("draw:frame");
    n.append_attribute("text:anchor-type") = "as-char";
    if(!f.style.is_empty())
        n.append_attribute("draw:style-name").set_value(f.style.get_name().c_str());
    if(f.width)
        n.append_attribute("svg:width") = (detail::to_string(f.width->m_points) + "pt").c_str();
    if(f.height)
        n.append_attribute("svg:height") = (detail::to_string(f.height->m_points) + "pt").c_str();
    emit(n, f.img);
    if(!f.accessibility_title.empty())
        n.append_child("svg:title").text() = f.accessibility_title.c_str();
    if(!f.accessibility_description.empty())
        n.append_child("svg:desc").text() = f.accessibility_description.c_str();
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

// The text in a field element is its value when the file was written, which
// readers show until they update it. Page numbers aren't known without laying
// the document out, so they are written as 1.
void writer::emit(pugi::xml_node parent, const field &f)
{
    pugi::xml_node n;
    std::string value = "1";
    switch(f.type)
    {
    case field_type::page_number:
        n = parent.append_child("text:page-number");
        n.append_attribute("text:select-page") = "current";
        break;
    case field_type::page_count: n = parent.append_child("text:page-count"); break;
    case field_type::date:
        value = detail::today();
        n = parent.append_child("text:date");
        n.append_attribute("style:data-style-name") = date_style_name;
        n.append_attribute("text:date-value") = value.c_str();
        if(!m_styles.find_child_by_attribute("number:date-style", "style:name", date_style_name))
            m_styles.append_buffer(date_style, std::strlen(date_style));
        break;
    case field_type::title:
        n = parent.append_child("text:title");
        value = m_doc->meta.title;
        break;
    }
    if(!value.empty())
        n.append_child(pugi::node_pcdata).set_value(value.c_str());
}

// LibreOffice shows the entries stored in the file and doesn't update an index
// when it opens one, so they are written here: a link to each heading (its
// bookmark, else LibreOffice's "#<text>|outline"). The page numbers need a
// layout; LibreOffice's Update Index adds them, from the entry templates.
void writer::emit(pugi::xml_node parent, const table_of_contents &toc)
{
    flush_page_break(parent);
    const int levels = std::clamp(toc.levels, 1, 10);
    define_style("Contents_20_Heading", contents_heading_style);
    for(int level = 1; level <= levels; ++level)
        define_style("Contents_20_" + std::to_string(level), contents_style(level));

    auto n = parent.append_child("text:table-of-content");
    const std::string name = "Table of Contents" + std::to_string(++m_tocs);
    n.append_attribute("text:protected") = "true";
    n.append_attribute("text:name") = name.c_str();

    auto source = n.append_child("text:table-of-content-source");
    source.append_attribute("text:outline-level") = levels;
    if(!toc.title.empty())
    {
        auto t = source.append_child("text:index-title-template");
        t.append_attribute("text:style-name") = "Contents_20_Heading";
        t.append_child(pugi::node_pcdata).set_value(toc.title.c_str());
    }
    for(int level = 1; level <= levels; ++level)
    {
        auto t = source.append_child("text:table-of-content-entry-template");
        t.append_attribute("text:outline-level") = level;
        t.append_attribute("text:style-name") = ("Contents_20_" + std::to_string(level)).c_str();
        t.append_child("text:index-entry-link-start");
        t.append_child("text:index-entry-chapter");
        t.append_child("text:index-entry-text");
        auto tab = t.append_child("text:index-entry-tab-stop");
        tab.append_attribute("style:type") = "right";
        tab.append_attribute("style:leader-char") = ".";
        t.append_child("text:index-entry-page-number");
        t.append_child("text:index-entry-link-end");
    }

    auto body = n.append_child("text:index-body");
    if(!toc.title.empty())
    {
        auto title = body.append_child("text:index-title");
        title.append_attribute("text:name") = (name + "_Head").c_str();
        auto p = title.append_child("text:p");
        p.append_attribute("text:style-name") = "Contents_20_Heading";
        p.append_child(pugi::node_pcdata).set_value(toc.title.c_str());
    }
    for(const heading *h : detail::body_headings(*m_doc))
    {
        if(h->level > levels)
            continue;
        const std::string text = detail::plain_text(h->children());
        const bookmark *b = detail::first_bookmark(h->children());
        const std::string href = "#" + (b ? b->name : text + "|outline");

        auto p = body.append_child("text:p");
        p.append_attribute("text:style-name") =
            ("Contents_20_" + std::to_string(std::max(h->level, 1))).c_str();
        auto a = p.append_child("text:a");
        a.append_attribute("xlink:type") = "simple";
        a.append_attribute("xlink:href") = href.c_str();
        emit(a, docsmith::text{text});
    }
}

/// Adds xml to styles.xml's common styles unless a paragraph style of that
/// name is defined there already.
void writer::define_style(const std::string &name, const std::string &xml)
{
    if(!find_style(m_styles, name, style_family::paragraph))
        m_styles.append_buffer(xml.data(), xml.size());
}

void writer::emit(pugi::xml_node parent, const page_break &)
{
    flush_page_break(parent); // two in a row: the first gets its own paragraph
    m_page_break_pending = true;
}

void writer::emit(pugi::xml_node parent, const unknown &u)
{
    if(u.source != markup::odf)
        return; // another format's markup
    flush_page_break(parent);
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
    if(l.continue_numbering)
        n.append_attribute("text:continue-numbering") = "true";
    // Only the first item can be unnumbered (text:list-header); a later one is
    // more of the item before it, as LibreOffice saves one.
    for(const auto &it : l.items())
        if(!it.numbered && n.last_child())
            write_blocks(n.last_child(), it.children());
        else
            emit(n, it);
}

void writer::emit(pugi::xml_node parent, const list_item &it)
{
    auto n = parent.append_child(it.numbered ? "text:list-item" : "text:list-header");
    if(it.numbered && it.start_value)
        n.append_attribute("text:start-value") = *it.start_value;
    write_blocks(n, it.children());
}

std::string writer::automatic_name(const std::string &prefix, style_family f)
{
    for(;;)
        if(std::string name = prefix + std::to_string(++m_automatic_names);
            !find_style(m_automatic_styles, name, f) && !m_doc->styles().find(name))
            return name;
}

// The cell's style, unless its table's style or its direct formatting adds to
// it: then an automatic style with all of them combined, the cell's own style
// as its parent. Cells that look the same share one.
std::string writer::cell_style(const table &t, size_t r, size_t c)
{
    const table_cell &cell = t.cell(r, c);
    // A cell without a style of its own is based on its column's.
    const style_name &base = cell.style.is_empty() ? t.column_style(c) : cell.style;
    cell_props props = effective_cell_props(t, m_doc->styles(), r, c);
    props.m_paragraph_style = {}; // on the cell's paragraphs instead
    // LibreOffice draws both cells' edges where two meet, so a line between
    // them is drawn once, as the cell above's bottom (or the cell before's
    // end), as in its own files.
    auto drawn = [&](size_t nr, size_t nc, std::optional<border> cell_borders::*side)
    {
        const auto [orow, ocol] = t.origin(nr, nc);
        const auto b = effective_cell_props(t, m_doc->styles(), orow, ocol).m_borders.*side;
        return b && b->m_line != border_line::none;
    };
    if(r != 0 && props.m_borders.m_top && drawn(r - 1, c, &cell_borders::m_bottom))
        props.m_borders.m_top = border::none();
    if(c != 0 && props.m_borders.m_start && drawn(r, c - 1, &cell_borders::m_end))
        props.m_borders.m_start = border::none();
    cell_props base_props = effective_cell_style(base, m_doc->styles());
    base_props.m_paragraph_style = {};
    if(props == base_props)
        return base.get_name();
    for(const auto &[parent, p, name] : m_cell_styles)
        if(parent == base && p == props)
            return name;

    style s(style_name(automatic_name("Cell", style_family::table_cell)), style_family::table_cell);
    s.m_parent_style = base;
    s.set(props);
    write_xml(m_automatic_styles, s);
    m_cell_styles.emplace_back(base, props, s.m_name.get_name());
    return s.m_name.get_name();
}

void writer::emit(pugi::xml_node parent, const table &t)
{
    flush_page_break(parent); // a break in a cell would have no effect
    auto n = parent.append_child("table:table");
    if(!t.name.empty()) // LibreOffice names the others when it loads them
        n.append_attribute("table:name") = t.name.c_str();
    if(!t.style.is_empty())
        n.append_attribute("table:style-name") = t.style.get_name().c_str();
    if(!t.accessibility_title.empty())
        n.append_child("table:title").text() = t.accessibility_title.c_str();
    if(!t.accessibility_description.empty())
        n.append_child("table:desc").text() = t.accessibility_description.c_str();

    // Columns with a width get an automatic style for it. Without widths,
    // neighbouring columns with the same style are written once, repeated.
    const table_props tp = effective_table_props(t, m_doc->styles());
    bool widths = false;
    for(size_t c = 0; c != t.num_cols(); ++c)
        widths = widths || t.column_width(c);
    const auto shares = column_shares(t);
    // Relative widths as given when they are whole numbers of LibreOffice's
    // size (as read from a file), so they read back the same; else the shares,
    // scaled. LibreOffice ignores small ones such as 3* and 1*.
    bool whole = true;
    double total = 0;
    for(size_t c = 0; c != t.num_cols(); ++c)
    {
        const auto w = t.column_width(c);
        whole = whole && w && *w >= 1 && *w == static_cast<double>(static_cast<long>(*w));
        total += w.value_or(0);
    }
    whole = whole && total >= 1000;
    double width = 17 * 72 / 2.54; // the default page's text width, in points
    if(tp.m_width)
        width = std::holds_alternative<length>(*tp.m_width)
            ? std::get<length>(*tp.m_width).m_points
            : width * std::get<percent>(*tp.m_width).m_value / 100;
    for(size_t c = 0; c != t.num_cols();)
    {
        size_t same = 1;
        while(!widths && c + same != t.num_cols() && t.column_style(c + same) == t.column_style(c))
            ++same;
        auto col = n.append_child("table:table-column");
        if(widths)
        {
            const std::string name = automatic_name("Column", style_family::table_column);
            auto s = m_automatic_styles.append_child("style:style");
            s.append_attribute("style:name") = name.c_str();
            s.append_attribute("style:family") = "table-column";
            auto props = s.append_child("style:table-column-properties");
            props.append_attribute("style:column-width") =
                (detail::to_string(width * shares[c]) + "pt").c_str();
            const long rel = whole ? static_cast<long>(*t.column_width(c))
                                   : static_cast<long>(shares[c] * 65535 + 0.5);
            props.append_attribute("style:rel-column-width") = (std::to_string(rel) + "*").c_str();
            col.append_attribute("table:style-name") = name.c_str();
        }
        if(!t.column_style(c).is_empty())
            col.append_attribute("table:default-cell-style-name") =
                t.column_style(c).get_name().c_str();
        if(same != 1)
            col.append_attribute("table:number-columns-repeated")
                .set_value(static_cast<unsigned>(same));
        c += same;
    }

    // Unstyled paragraphs in a cell get its paragraph style (cell_props).
    const style_name saved = m_cell_paragraph_style;
    auto header = t.header_rows() ? n.append_child("table:table-header-rows") : pugi::xml_node{};
    for(size_t r = 0; r != t.num_rows(); ++r)
    {
        auto xml_row = (r < t.header_rows() ? header : n).append_child("table:table-row");
        if(!t.row_style(r).is_empty())
            xml_row.append_attribute("table:style-name") = t.row_style(r).get_name().c_str();
        for(size_t c = 0; c != t.num_cols(); ++c)
        {
            if(t.is_cell_covered(r, c))
                xml_row.append_child("table:covered-table-cell");
            else
            {
                m_cell_paragraph_style =
                    effective_cell_props(t, m_doc->styles(), r, c).m_paragraph_style;
                emit(xml_row, t.cell(r, c), cell_style(t, r, c));
            }
        }
    }
    m_cell_paragraph_style = saved;
}

void writer::emit(pugi::xml_node parent, const table_cell &c, const std::string &style)
{
    auto n = parent.append_child("table:table-cell");
    if(!style.empty())
        n.append_attribute("table:style-name").set_value(style.c_str());

    if(c.rowspan != 1)
        n.append_attribute("table:number-rows-spanned").set_value(c.rowspan);
    if(c.colspan != 1)
        n.append_attribute("table:number-columns-spanned").set_value(c.colspan);

    write_blocks(n, c.children());
}

} // namespace docsmith::odt
