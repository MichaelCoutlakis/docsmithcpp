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
#include "docx/writer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>

#include "bytes.h"
#include "docx/meta.h"
#include "docx/ooxml.h"
#include "docx/styles.h"
#include "format.h"
#include "image_size.h"
#include "outline.h"

namespace docsmith::docx
{
namespace fs = std::filesystem;

namespace
{
constexpr long long emu_per_inch = 914400;
// The text width of an A4 page with 2cm margins: 17cm, in EMUs and twips.
constexpr long long text_width_emu = 6120000;
constexpr int text_width_twips = 9638;

std::string media_type_of(const fs::path &p)
{
    std::string ext = p.extension().string();
    std::transform(ext.begin(),
        ext.end(),
        ext.begin(),
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
    return "application/octet-stream";
}

constexpr const char *base_styles =
    R"~~(<w:styles><w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii="Times New Roman" w:hAnsi="Times New Roman" w:eastAsia="Times New Roman" w:cs="Times New Roman"/><w:sz w:val="24"/><w:szCs w:val="24"/></w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:spacing w:after="140" w:line="276" w:lineRule="auto"/></w:pPr></w:pPrDefault></w:docDefaults><w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:qFormat/></w:style><w:style w:type="character" w:default="1" w:styleId="DefaultParagraphFont"><w:name w:val="Default Paragraph Font"/><w:uiPriority w:val="1"/><w:semiHidden/><w:unhideWhenUsed/></w:style><w:style w:type="table" w:default="1" w:styleId="TableNormal"><w:name w:val="Normal Table"/><w:uiPriority w:val="99"/><w:semiHidden/><w:unhideWhenUsed/><w:tblPr><w:tblInd w:w="0" w:type="dxa"/><w:tblCellMar><w:top w:w="0" w:type="dxa"/><w:left w:w="108" w:type="dxa"/><w:bottom w:w="0" w:type="dxa"/><w:right w:w="108" w:type="dxa"/></w:tblCellMar></w:tblPr></w:style></w:styles>)~~";

constexpr const char *base_settings =
    R"~~(<w:settings><w:defaultTabStop w:val="709"/><w:characterSpacingControl w:val="doNotCompress"/><w:compat><w:compatSetting w:name="compatibilityMode" w:uri="http://schemas.microsoft.com/office/word" w:val="15"/></w:compat></w:settings>)~~";

// The settings w:updateFields goes before (CT_Settings is a sequence).
constexpr std::array<const char *, 12> after_update_fields{"w:hdrShapeDefaults",
    "w:footnotePr",
    "w:endnotePr",
    "w:compat",
    "w:docVars",
    "w:rsids",
    "m:mathPr",
    "w:attachedSchema",
    "w:themeFontLang",
    "w:clrSchemeMapping",
    "w:doNotIncludeSubdocsInStats",
    "w:shapeDefaults"};

// Default page: A4, 2cm margins, as the ODT writer has.
constexpr const char *base_sect_pr =
    R"~~(<w:sectPr><w:pgSz w:w="11906" w:h="16838"/><w:pgMar w:top="1134" w:right="1134" w:bottom="1134" w:left="1134" w:header="567" w:footer="567" w:gutter="0"/><w:cols w:space="720"/></w:sectPr>)~~";

constexpr const char *sans = R"~~(<w:rFonts w:ascii="Arial" w:hAnsi="Arial" w:cs="Arial"/>)~~";
constexpr const char *mono =
    R"~~(<w:rFonts w:ascii="Courier New" w:hAnsi="Courier New" w:cs="Courier New"/>)~~";

std::string paragraph_style(
    const std::string &id, const std::string &name, const std::string &ppr, const std::string &rpr)
{
    return R"~~(<w:style w:type="paragraph" w:styleId=")~~" + id + R"~~("><w:name w:val=")~~" +
        name + R"~~("/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:qFormat/><w:pPr>)~~" +
        ppr + "</w:pPr><w:rPr>" + rpr + "</w:rPr></w:style>";
}

std::string size(int half_points)
{
    const std::string v = std::to_string(half_points);
    return R"~~(<w:sz w:val=")~~" + v + R"~~("/><w:szCs w:val=")~~" + v + R"~~("/>)~~";
}

/// Definitions for the styles the writer uses without the document defining
/// them: the builtins, the default heading styles, and those of the table of
/// contents and links. Word knows most by name, but other readers may not.
std::string builtin_style(const std::string &id)
{
    constexpr const char *keep = R"~~(<w:keepNext/><w:spacing w:before="240" w:after="120"/>)~~";
    if(id == builtin::title.get_name())
        return paragraph_style(id,
            "Title",
            std::string(keep) + R"~~(<w:jc w:val="center"/>)~~",
            std::string(sans) + "<w:b/><w:bCs/>" + size(56));
    if(id == builtin::preformatted.get_name())
        return paragraph_style(id,
            "Preformatted Text",
            R"~~(<w:spacing w:before="0" w:after="0" w:line="240" w:lineRule="auto"/>)~~",
            std::string(mono) + size(20));
    if(id == builtin::source_text.get_name())
        return R"~~(<w:style w:type="character" w:styleId=")~~" + id +
            R"~~("><w:name w:val="Source Text"/><w:qFormat/><w:rPr>)~~" + mono +
            "</w:rPr></w:style>";
    if(id == "Hyperlink")
        return R"~~(<w:style w:type="character" w:styleId="Hyperlink"><w:name w:val="Hyperlink"/><w:rPr><w:color w:val="0563C1"/><w:u w:val="single"/></w:rPr></w:style>)~~";
    if(id == "TOCHeading")
        return paragraph_style(
            id, "TOC Heading", keep, std::string(sans) + "<w:b/><w:bCs/>" + size(32));
    if(id.size() == 8 && id.starts_with("Heading") && id[7] >= '1' && id[7] <= '9')
    {
        const int level = id[7] - '0';
        constexpr int sizes[] = {36, 32, 28, 26, 24, 22, 22, 20, 20};
        return paragraph_style(id,
            "heading " + id.substr(7),
            std::string(keep) + R"~~(<w:outlineLvl w:val=")~~" + std::to_string(level - 1) +
                R"~~("/>)~~",
            std::string(sans) + "<w:b/><w:bCs/>" + size(sizes[level - 1]));
    }
    if(id.size() == 4 && id.starts_with("TOC") && id[3] >= '1' && id[3] <= '9')
    {
        const int level = id[3] - '0';
        return paragraph_style(id,
            "toc " + id.substr(3),
            R"~~(<w:tabs><w:tab w:val="right" w:leader="dot" w:pos=")~~" +
                std::to_string(text_width_twips) +
                R"~~("/></w:tabs><w:spacing w:after="100"/><w:ind w:left=")~~" +
                std::to_string(283 * (level - 1)) + R"~~("/>)~~",
            "");
    }
    return {};
}

/// The first descendant element of n named name.
pugi::xml_node find_descendant(const pugi::xml_node &n, std::string_view name)
{
    for(auto c : n.children())
    {
        if(c.type() != pugi::node_element)
            continue;
        if(name == c.name())
            return c;
        if(auto d = find_descendant(c, name))
            return d;
    }
    return {};
}

template <typename F>
void for_each_element(const pugi::xml_node &n, F f)
{
    for(auto c : n.children())
        if(c.type() == pugi::node_element)
        {
            f(c);
            for_each_element(c, f);
        }
}

/// The values of every r: attribute in n: the relationships the part uses.
void referenced_ids(const pugi::xml_node &n, std::set<std::string> &out)
{
    for_each_element(n,
        [&](const pugi::xml_node &e)
        {
            for(auto a : e.attributes())
                if(std::string_view(a.name()).starts_with("r:"))
                    out.insert(a.value());
        });
}

void set_val(pugi::xml_node n, const std::string &v)
{
    if(!n.attribute("w:val"))
        n.append_attribute("w:val");
    n.attribute("w:val").set_value(v.c_str());
}

/// The families each style name is used with: paragraph for paragraphs and
/// headings, text for spans.
std::map<std::string, std::set<style_family>> used_families(const text_doc &doc)
{
    std::map<std::string, std::set<style_family>> out;
    walk(doc,
        [&out](const auto &n)
        {
            using T = std::decay_t<decltype(n)>;
            if constexpr(std::is_same_v<T, paragraph> || std::is_same_v<T, heading>)
            {
                if(!n.style.is_empty())
                    out[n.style.get_name()].insert(style_family::paragraph);
            }
            else if constexpr(std::is_same_v<T, span>)
            {
                if(!n.style.is_empty())
                    out[n.style.get_name()].insert(style_family::text);
            }
        });
    return out;
}

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

class writer
{
public:
    writer(const text_doc &doc, state &st) :
        m_doc(doc), m_st(st), m_pkg(st.pkg), m_used(used_families(doc))
    {
        for(const auto &[name, families] : m_used)
            if(const style *s = doc.styles().find(name); s && !s->m_family && !is_direct(name) &&
                families.contains(style_family::paragraph) && families.contains(style_family::text))
                m_dual.insert(name);
    }

    void write()
    {
        const bool is_new = m_st.main_part.empty() || !m_pkg.contains(m_st.main_part);
        if(is_new)
        {
            m_st.main_part = "word/document.xml";
            m_pkg.ensure_rel("", rel::office_document, m_st.main_part);
        }
        m_main = m_st.main_part;
        load_parts();
        plan_toc();

        m_part = m_main;
        auto body = m_document.document_element().child("w:body");
        write_blocks(body, m_doc.body);
        body.append_copy(m_sect_pr.document_element());
        write_page();

        write_styles();
        write_numbering();
        write_settings();
        collect_garbage();

        m_pkg.set(m_main, xml::save(m_document), content_type::document);
        for(const auto &[name, part] : m_parts)
            m_pkg.set(name, xml::save(*part.xml), part.content_type);
        write_meta(m_doc.meta, m_pkg, is_new);
    }

private:
    struct part_xml
    {
        std::unique_ptr<pugi::xml_document> xml;
        const char *content_type;
    };

    /// What a run in the current context looks like.
    struct run_format
    {
        std::string style;
        bool in_link{false};
    };

    /// The list a paragraph belongs to, when writing list items.
    struct list_context
    {
        int ilvl{0};
        int num_id{0}; //!< 0: a later paragraph of the item
    };

    // ---- parts ----

    /// The part a relationship of the main document of that type points to.
    std::string related_part(const char *type) const
    {
        const auto *r = m_pkg.find_rel_by_type(m_main, type);
        return r ? opc::package::resolve(m_main, r->target) : std::string();
    }

    bool load_part(pugi::xml_document &xml, const std::string &name, const char *root)
    {
        const auto *p = name.empty() ? nullptr : m_pkg.find(name);
        return p && load(xml, p->data) && xml.child(root);
    }

    pugi::xml_document &part(const std::string &name, const char *content_type)
    {
        auto &p = m_parts[name];
        if(!p.xml)
            p = part_xml{std::make_unique<pugi::xml_document>(), content_type};
        return *p.xml;
    }

    void load_parts()
    {
        if(load_part(m_document, m_main, "w:document"))
        {
            auto body = m_document.child("w:document").child("w:body");
            // Ids the writer gives must not clash with those in kept markup.
            for_each_element(body,
                [&](const pugi::xml_node &e)
                {
                    const std::string_view name = e.name();
                    if(name == "w:bookmarkStart")
                        m_bookmark_id = std::max(m_bookmark_id, e.attribute("w:id").as_int() + 1);
                    else if(name == "wp:docPr")
                        m_drawing_id = std::max(m_drawing_id, e.attribute("id").as_int() + 1);
                });
            if(auto s = body.last_child(); std::string_view(s.name()) == "w:sectPr")
                m_sect_pr.append_copy(s);
            if(!body)
                body = m_document.child("w:document").append_child("w:body");
            while(body.first_child())
                body.remove_child(body.first_child());
        }
        else
        {
            m_document.reset();
            m_document.append_child("w:document").append_child("w:body");
        }
        xml::declare(m_document.document_element(), body_namespaces);
        if(!m_sect_pr.document_element())
            m_sect_pr.load_string(base_sect_pr);

        m_styles_name = related_part(rel::styles);
        if(!load_part(m_styles, m_styles_name, "w:styles"))
        {
            m_styles.load_string(base_styles);
            m_styles_name = "word/styles.xml";
        }
        xml::declare(m_styles.document_element(), body_namespaces.first(1));

        m_numbering_name = related_part(rel::numbering);
        if(!load_part(m_numbering, m_numbering_name, "w:numbering"))
        {
            m_numbering.reset();
            m_numbering.append_child("w:numbering");
            m_numbering_name = "word/numbering.xml";
        }
        xml::declare(m_numbering.document_element(), body_namespaces.first(1));
        for(auto a : m_numbering.document_element().children("w:abstractNum"))
            m_next_abstract =
                std::max(m_next_abstract, a.attribute("w:abstractNumId").as_int() + 1);
        for(auto n : m_numbering.document_element().children("w:num"))
            m_next_num = std::max(m_next_num, n.attribute("w:numId").as_int() + 1);

        m_settings_name = related_part(rel::settings);
        if(!load_part(m_settings, m_settings_name, "w:settings"))
        {
            m_settings.load_string(base_settings);
            m_settings_name = "word/settings.xml";
        }
        xml::declare(m_settings.document_element(), body_namespaces.first(1));
    }

    /// A part name like word/header1.xml not taken yet.
    std::string new_part_name(const std::string &stem)
    {
        for(int i = 1;; ++i)
        {
            std::string name = "word/" + stem + std::to_string(i) + ".xml";
            if(!m_pkg.contains(name) && !m_parts.contains(name))
                return name;
        }
    }

    // The header and footer are the default ones of the body's section. The
    // others (first page, even pages) are kept as they are.
    void write_page()
    {
        auto body = m_document.document_element().child("w:body");
        auto sect = body.last_child();
        auto write = [&](const char *ref,
                         const char *type,
                         const char *root,
                         const char *stem,
                         const char *ct,
                         const std::vector<block_node> &blocks)
        {
            std::string name;
            for(auto r = sect.child(ref); r;)
            {
                auto next = r.next_sibling(ref);
                if(std::string_view(r.attribute("w:type").as_string("default")) == "default")
                {
                    if(const auto *rel = m_pkg.find_rel(m_main, r.attribute("r:id").as_string()))
                        name = opc::package::resolve(m_main, rel->target);
                    sect.remove_child(r);
                }
                r = next;
            }
            if(blocks.empty())
                return;
            if(name.empty())
                name = new_part_name(stem);

            auto &xml = part(name, ct);
            if(!load_part(xml, name, root))
            {
                xml.reset();
                xml.append_child(root);
            }
            auto r = xml.child(root);
            xml::declare(r, body_namespaces);
            while(r.first_child())
                r.remove_child(r.first_child());
            m_part = name;
            write_blocks(r, blocks);
            m_part = m_main;

            // References come first in a section, headers before footers.
            auto n = sect.prepend_child(ref);
            pugi::xml_node last_header;
            for(auto c : sect.children("w:headerReference"))
                last_header = c;
            if(last_header && last_header != n)
                n = sect.insert_move_after(n, last_header);
            n.append_attribute("w:type") = "default";
            n.append_attribute("r:id") =
                m_pkg.ensure_rel(m_main, type, opc::package::relative(m_main, name)).c_str();
        };
        write("w:headerReference",
            rel::header,
            "w:hdr",
            "header",
            content_type::header,
            m_doc.page.header);
        write("w:footerReference",
            rel::footer,
            "w:ftr",
            "footer",
            content_type::footer,
            m_doc.page.footer);
    }

    // ---- styles ----

    /// Whether name is direct formatting as read, to be written as such.
    bool is_direct(const std::string &name) const
    {
        auto it = m_st.direct.find(name);
        if(it == m_st.direct.end())
            return false;
        const style *s = m_doc.styles().find(name);
        return !s || *s == it->second.def;
    }

    /// The w:rStyle for a span's style.
    std::string char_id(const std::string &name) const
    { return m_dual.contains(name) ? name + "Char" : name; }

    pugi::xml_node find_style(const std::string &id, std::string_view type) const
    {
        for(auto s : m_styles.document_element().children("w:style"))
            if(id == s.attribute("w:styleId").as_string() &&
                type == s.attribute("w:type").as_string("paragraph"))
                return s;
        return {};
    }

    void write_styles()
    {
        auto root = m_styles.document_element();
        for(const style *s : sorted(m_doc.styles()))
        {
            const std::string &name = s->m_name.get_name();
            if(is_direct(name))
                continue;
            std::set<style_family> families;
            if(s->m_family)
                families = {*s->m_family};
            else if(auto it = m_used.find(name); it != m_used.end())
                families = it->second;
            else
                families = {style_family::paragraph};

            const bool dual = m_dual.contains(name);
            for(style_family f : families)
            {
                const char *type = style_type(f);
                if(!type)
                    continue;
                const bool as_char = dual && f == style_family::text;
                const std::string id = as_char ? char_id(name) : name;
                const std::string link = dual ? (as_char ? name : char_id(name)) : std::string();
                std::string based_on = s->m_parent_style.get_name();
                if(f == style_family::text)
                    based_on = char_id(based_on);

                style def = *s;
                def.m_family = f;
                auto existing = find_style(id, type);
                if(existing)
                    if(auto old = read_style(existing))
                    {
                        old->m_name = def.m_name;
                        if(*old == def)
                            continue;
                    }
                pugi::xml_node n = existing ? root.insert_child_before("w:style", existing)
                                            : root.append_child("w:style");
                write_style(n, def, type, id, based_on, link);
                if(existing)
                    root.remove_child(existing);
            }
        }

        for(const auto &[id, type] : m_used_ids)
            if(!find_style(id, type))
                if(const std::string def = builtin_style(id); !def.empty())
                    root.append_buffer(def.data(), def.size());

        m_parts.erase(m_styles_name);
        part(m_styles_name, content_type::styles).reset(m_styles);
        m_pkg.ensure_rel(m_main, rel::styles, opc::package::relative(m_main, m_styles_name));
    }

    // ---- numbering ----

    /// The abstractNum for a list style name, added (or updated) as needed.
    int abstract_num(const std::string &name)
    {
        if(auto it = m_abstract_ids.find(name); it != m_abstract_ids.end())
            return it->second;

        auto root = m_numbering.document_element();
        const std::string wanted = name.empty() ? default_list_name : name;
        pugi::xml_node existing;
        for(auto a : root.children("w:abstractNum"))
        {
            const std::string own = val(a, "w:name");
            const std::string fallback =
                "WWNum" + std::string(a.attribute("w:abstractNumId").as_string());
            if(own == wanted || (own.empty() && fallback == wanted))
                existing = a;
        }

        const list_style *ls = name.empty() ? nullptr : m_doc.list_styles().find(name);
        int id = existing ? existing.attribute("w:abstractNumId").as_int() : m_next_abstract++;
        if(existing && (!ls || read_abstract_num(existing, style_name(name)) == *ls))
            return m_abstract_ids[name] = id;

        pugi::xml_node before = existing ? existing : root.child("w:num");
        if(!before)
            before = root.child("w:numIdMacAtCleanup");
        auto n = before ? root.insert_child_before("w:abstractNum", before)
                        : root.append_child("w:abstractNum");
        if(ls)
            write_abstract_num(n, id, *ls);
        else
        {
            static const char *const bullets[] = {"•", "◦", "▪"};
            list_style def(style_name(wanted), {});
            for(int level = 1; level <= 9; ++level)
                def.m_level_styles.emplace_back(
                    list_style_bullet(style{}, level, bullets[(level - 1) % 3]));
            write_abstract_num(n, id, def);
        }
        if(existing)
            root.remove_child(existing);
        return m_abstract_ids[name] = id;
    }

    /// A new w:num for one list, so its numbering starts again.
    int new_num(const std::string &list_style_name)
    {
        const int abstract = abstract_num(list_style_name);
        auto root = m_numbering.document_element();
        auto before = root.child("w:numIdMacAtCleanup");
        auto n = before ? root.insert_child_before("w:num", before) : root.append_child("w:num");
        const int id = m_next_num++;
        n.append_attribute("w:numId") = id;
        set_val(n.append_child("w:abstractNumId"), std::to_string(abstract));
        if(const list_style *ls = m_doc.list_styles().find(list_style_name))
            for(const auto &lvl : ls->m_level_styles)
                if(const auto *num = std::get_if<list_style_num>(&lvl))
                {
                    auto o = n.append_child("w:lvlOverride");
                    o.append_attribute("w:ilvl") = std::max(num->m_level - 1, 0);
                    set_val(o.append_child("w:startOverride"), std::to_string(num->m_start_from));
                }
        return id;
    }

    void write_numbering()
    {
        // Drop the instances (and definitions) nothing refers to, such as the
        // ones a loaded file's lists had: each list written gets a new one.
        std::set<std::string> used;
        auto collect = [&](const pugi::xml_node &n)
        {
            for_each_element(n,
                [&](const pugi::xml_node &e)
                {
                    if(std::string_view(e.name()) == "w:numId")
                        used.insert(e.attribute("w:val").as_string());
                });
        };
        collect(m_document);
        collect(m_styles);
        for(const auto &[name, p] : m_parts)
            collect(*p.xml);
        for(const auto &[name, p] : m_pkg.parts())
            if(!m_parts.contains(name) && name != m_main && name != m_numbering_name &&
                p.content_type.ends_with("+xml"))
            {
                pugi::xml_document other;
                if(load(other, p.data))
                    collect(other);
            }

        auto root = m_numbering.document_element();
        std::set<std::string> abstracts;
        for(auto n = root.child("w:num"); n;)
        {
            auto next = n.next_sibling("w:num");
            if(!used.contains(n.attribute("w:numId").as_string()))
                root.remove_child(n);
            else
                abstracts.insert(val(n, "w:abstractNumId"));
            n = next;
        }
        for(auto a = root.child("w:abstractNum"); a;)
        {
            auto next = a.next_sibling("w:abstractNum");
            if(!abstracts.contains(a.attribute("w:abstractNumId").as_string()) &&
                !a.child("w:numStyleLink") && !a.child("w:styleLink"))
                root.remove_child(a);
            a = next;
        }

        const bool had = m_pkg.contains(m_numbering_name);
        if(!root.child("w:num") && !had)
            return;
        part(m_numbering_name, content_type::numbering).reset(m_numbering);
        m_pkg.ensure_rel(m_main, rel::numbering, opc::package::relative(m_main, m_numbering_name));
    }

    // ---- settings ----

    void write_settings()
    {
        auto root = m_settings.document_element();
        if(detail::has_toc(m_doc) && !root.child("w:updateFields"))
        {
            pugi::xml_node before;
            for(auto c : root.children())
                if(std::find_if(after_update_fields.begin(),
                       after_update_fields.end(),
                       [&](const char *n)
                       { return std::string_view(n) == c.name(); }) != after_update_fields.end())
                {
                    before = c;
                    break;
                }
            auto n = before ? root.insert_child_before("w:updateFields", before)
                            : root.append_child("w:updateFields");
            n.append_attribute("w:val") = "true";
        }
        part(m_settings_name, content_type::settings).reset(m_settings);
        m_pkg.ensure_rel(m_main, rel::settings, opc::package::relative(m_main, m_settings_name));
    }

    // ---- relationships no longer used ----

    void collect_garbage()
    {
        auto prune = [&](const std::string &source, const pugi::xml_node &xml)
        {
            std::set<std::string> ids;
            referenced_ids(xml, ids);
            std::erase_if(m_pkg.rels(source),
                [&](const opc::relationship &r)
                {
                    return (r.type == rel::hyperlink || r.type == rel::image ||
                               r.type == rel::header || r.type == rel::footer) &&
                        !ids.contains(r.id);
                });
        };
        prune(m_main, m_document);
        for(const auto &[name, p] : m_parts)
            if(p.content_type == content_type::header || p.content_type == content_type::footer)
                prune(name, *p.xml);

        // Headers, footers and images no relationship points to any more.
        std::set<std::string> sources{"", m_main}, targets;
        for(const auto &[name, p] : m_pkg.parts())
            sources.insert(name);
        for(const auto &[name, p] : m_parts)
            sources.insert(name);
        for(const auto &source : sources)
            if(const auto *rels = m_pkg.find_rels(source))
                for(const auto &r : *rels)
                    if(!r.external)
                        targets.insert(opc::package::resolve(source, r.target));
        std::vector<std::string> unused;
        for(const auto &[name, p] : m_pkg.parts())
            if(!targets.contains(name) &&
                (p.content_type == content_type::header || p.content_type == content_type::footer ||
                    p.content_type.starts_with("image/")))
                unused.push_back(name);
        for(const auto &[name, p] : m_parts)
            if(!targets.contains(name) &&
                (p.content_type == content_type::header || p.content_type == content_type::footer))
                unused.push_back(name);
        for(const auto &name : unused)
        {
            m_pkg.take(name);
            m_parts.erase(name);
        }
    }

    // ---- table of contents ----

    /// The bookmark each body heading is linked to from a table of contents:
    /// its own, or a _Toc one the writer adds.
    void plan_toc()
    {
        if(!detail::has_toc(m_doc))
            return;
        std::set<std::string> taken;
        for(const bookmark *b : collect<bookmark>(m_doc))
            taken.insert(b->name);
        int n = 0;
        for(const heading *h : detail::body_headings(m_doc))
        {
            if(const bookmark *b = detail::first_bookmark(h->children()))
            {
                m_toc_anchor[h] = b->name;
                continue;
            }
            std::string name;
            do
                name = std::string(toc_bookmark_prefix) + std::to_string(++n);
            while(taken.contains(name));
            m_toc_anchor[h] = name;
            m_toc_own.insert(h);
        }
    }

    // ---- blocks ----

    void write_blocks(pugi::xml_node parent, const std::vector<block_node> &blocks)
    {
        for(const auto &b : blocks)
            std::visit([&](const auto &n) { emit(parent, n); }, b.v);
    }

    /// The w:pPr for a paragraph with that style, and the list it is in.
    pugi::xml_node paragraph_props(pugi::xml_node p, const style_name &sn)
    {
        pugi::xml_node ppr;
        std::string style_id = sn.get_name();
        if(is_direct(style_id))
        {
            const auto &d = m_st.direct.at(style_id);
            pugi::xml_document frag;
            if(frag.load_buffer(
                   d.xml.data(), d.xml.size(), pugi::parse_default | pugi::parse_ws_pcdata))
                ppr = p.append_copy(frag.document_element());
            style_id = d.def.m_parent_style.get_name();
        }
        if(!ppr)
            ppr = p.append_child("w:pPr");
        if(!style_id.empty())
        {
            set_val(ordered_child(ppr, "w:pStyle", ppr_order), style_id);
            m_used_ids.emplace(style_id, "paragraph");
        }
        if(m_list)
        {
            auto num = ordered_child(ppr, "w:numPr", ppr_order);
            set_val(num.append_child("w:ilvl"), std::to_string(m_list->ilvl));
            set_val(num.append_child("w:numId"), std::to_string(m_list->num_id));
            if(m_list->num_id == 0) // a later paragraph of the item: indented, no number
                ordered_child(ppr, "w:ind", ppr_order).append_attribute("w:left") =
                    list_indent(m_list->ilvl);
            m_list->num_id = 0;
        }
        return ppr;
    }

    static void drop_if_empty(pugi::xml_node p, pugi::xml_node ppr)
    {
        if(!ppr.first_child() && !ppr.first_attribute())
            p.remove_child(ppr);
    }

    void emit(pugi::xml_node parent, const paragraph &p)
    {
        auto n = parent.append_child("w:p");
        auto ppr = paragraph_props(n, p.style);
        drop_if_empty(n, ppr);
        write_inlines(n, p.children(), {});
    }

    void emit(pugi::xml_node parent, const heading &h)
    {
        auto n = parent.append_child("w:p");
        const style_name sn = h.style.is_empty() ? style_name(heading_style(h.level)) : h.style;
        auto ppr = paragraph_props(n, sn);
        set_val(ordered_child(ppr, "w:outlineLvl", ppr_order),
            std::to_string(std::clamp(h.level, 1, 9) - 1));
        const bool own = m_toc_own.contains(&h);
        int id = 0;
        if(own)
            id = bookmark_start(n, m_toc_anchor.at(&h));
        write_inlines(n, h.children(), {});
        if(own)
            n.append_child("w:bookmarkEnd").append_attribute("w:id") = id;
    }

    void emit(pugi::xml_node parent, const list &l)
    {
        // An unstyled nested list continues its parent's numbering one level
        // down; a styled one gets its own, at the same depth.
        const bool nested = m_list_depth > 0;
        const int num_id =
            (nested && l.style.is_empty()) ? m_list_num : new_num(l.style.get_name());
        const int saved_num = m_list_num;
        m_list_num = num_id;
        ++m_list_depth;
        for(const auto &item : l.items())
        {
            bool first = true;
            for(const auto &b : item.children())
            {
                const bool para =
                    std::holds_alternative<paragraph>(b.v) || std::holds_alternative<heading>(b.v);
                if(para)
                    m_list = list_context{m_list_depth - 1, first ? num_id : 0};
                std::visit([&](const auto &n) { emit(parent, n); }, b.v);
                m_list.reset();
                if(para)
                    first = false;
            }
            if(item.children().empty())
            {
                m_list = list_context{m_list_depth - 1, num_id};
                emit(parent, paragraph{});
                m_list.reset();
            }
        }
        --m_list_depth;
        m_list_num = saved_num;
    }

    void emit(pugi::xml_node parent, const table &t)
    {
        const std::size_t rows = t.num_rows(), cols = t.num_cols();
        if(cols == 0)
            return;
        auto tbl = parent.append_child("w:tbl");
        auto tbl_pr = tbl.append_child("w:tblPr");
        auto w = tbl_pr.append_child("w:tblW");
        w.append_attribute("w:w") = 5000;
        w.append_attribute("w:type") = "pct";
        auto look = tbl_pr.append_child("w:tblLook");
        look.append_attribute("w:val") = "04A0";
        const int col_width = text_width_twips / static_cast<int>(cols);
        auto grid = tbl.append_child("w:tblGrid");
        for(std::size_t c = 0; c != cols; ++c)
            grid.append_child("w:gridCol").append_attribute("w:w") = col_width;

        // The cell each slot belongs to.
        std::vector<std::vector<std::pair<std::size_t, std::size_t>>> origin(
            rows, std::vector<std::pair<std::size_t, std::size_t>>(cols, {SIZE_MAX, SIZE_MAX}));
        for(std::size_t r = 0; r != rows; ++r)
            for(std::size_t c = 0; c != cols && c < t.rows[r].size(); ++c)
            {
                if(origin[r][c].first != SIZE_MAX)
                    continue;
                const auto &cell = t.rows[r][c];
                for(std::size_t dr = 0;
                    dr < static_cast<std::size_t>(std::max(cell.rowspan, 1)) && r + dr < rows;
                    ++dr)
                    for(std::size_t dc = 0;
                        dc < static_cast<std::size_t>(std::max(cell.colspan, 1)) && c + dc < cols;
                        ++dc)
                        if(origin[r + dr][c + dc].first == SIZE_MAX)
                            origin[r + dr][c + dc] = {r, c};
            }

        auto saved = m_list;
        m_list.reset();
        for(std::size_t r = 0; r != rows; ++r)
        {
            auto tr = tbl.append_child("w:tr");
            if(r < t.header_rows)
                tr.append_child("w:trPr").append_child("w:tblHeader");
            for(std::size_t c = 0; c < cols;)
            {
                const auto o = origin[r][c].first == SIZE_MAX ? std::pair{r, c} : origin[r][c];
                std::size_t span = 1;
                while(c + span < cols && origin[r][c + span] == o)
                    ++span;
                if(o.second != c) // overlapping spans: covered already
                {
                    c += span;
                    continue;
                }
                const auto &cell = t.rows[o.first][o.second];
                auto tc = tr.append_child("w:tc");
                auto tc_pr = tc.append_child("w:tcPr");
                auto tc_w = tc_pr.append_child("w:tcW");
                tc_w.append_attribute("w:w") = col_width * static_cast<int>(span);
                tc_w.append_attribute("w:type") = "dxa";
                if(span > 1)
                    set_val(tc_pr.append_child("w:gridSpan"), std::to_string(span));
                if(cell.rowspan > 1)
                {
                    auto vm = tc_pr.append_child("w:vMerge");
                    if(o.first == r)
                        set_val(vm, "restart");
                }
                if(o.first == r)
                    write_blocks(tc, cell.children());
                // A cell ends with a paragraph.
                if(std::string_view(tc.last_child().name()) != "w:p")
                    tc.append_child("w:p");
                c += span;
            }
        }
        m_list = saved;
    }

    void emit(pugi::xml_node parent, const frame &f)
    {
        auto p = parent.append_child("w:p");
        auto r = p.append_child("w:r");
        write_drawing(r, f.img);
    }

    void emit(pugi::xml_node parent, const page_break &)
    {
        parent.append_child("w:p").append_child("w:r").append_child("w:br").append_attribute(
            "w:type") = "page";
    }

    void emit(pugi::xml_node parent, const table_of_contents &toc)
    {
        auto sdt = parent.append_child("w:sdt");
        auto obj = sdt.append_child("w:sdtPr").append_child("w:docPartObj");
        set_val(obj.append_child("w:docPartGallery"), "Table of Contents");
        obj.append_child("w:docPartUnique");
        auto content = sdt.append_child("w:sdtContent");

        if(!toc.title.empty())
        {
            auto p = content.append_child("w:p");
            set_val(p.append_child("w:pPr").append_child("w:pStyle"), "TOCHeading");
            m_used_ids.emplace("TOCHeading", "paragraph");
            emit_text(p, toc.title, {});
        }

        const int levels = std::clamp(toc.levels, 1, 9);
        const std::string instr = " TOC \\o \"1-" + std::to_string(levels) + "\" \\h \\z \\u ";
        auto field_char = [](pugi::xml_node p, const char *type)
        {
            p.append_child("w:r").append_child("w:fldChar").append_attribute("w:fldCharType") =
                type;
        };
        auto begin = [&](pugi::xml_node p)
        {
            field_char(p, "begin");
            auto t = p.append_child("w:r").append_child("w:instrText");
            t.append_attribute("xml:space") = "preserve";
            t.text().set(instr.c_str());
            field_char(p, "separate");
        };

        std::vector<const heading *> entries;
        for(const heading *h : detail::body_headings(m_doc))
            if(h->level <= levels)
                entries.push_back(h);
        if(entries.empty())
        {
            auto p = content.append_child("w:p");
            begin(p);
            field_char(p, "end");
            return;
        }
        for(std::size_t i = 0; i != entries.size(); ++i)
        {
            const heading *h = entries[i];
            const std::string id = "TOC" + std::to_string(std::clamp(h->level, 1, 9));
            auto p = content.append_child("w:p");
            set_val(p.append_child("w:pPr").append_child("w:pStyle"), id);
            m_used_ids.emplace(id, "paragraph");
            if(i == 0)
                begin(p);
            auto link = p.append_child("w:hyperlink");
            link.append_attribute("w:anchor") = m_toc_anchor.at(h).c_str();
            link.append_attribute("w:history") = "1";
            emit_text(link, detail::plain_text(h->children()), {});
            if(i + 1 == entries.size())
                field_char(p, "end");
        }
    }

    void emit(pugi::xml_node parent, const unknown &u) { append_unknown(parent, u); }

    void append_unknown(pugi::xml_node parent, const unknown &u)
    {
        if(u.source != markup::ooxml)
            return; // another format's markup
        pugi::xml_document frag;
        if(frag.load_buffer(u.xml.data(),
               u.xml.size(),
               pugi::parse_default | pugi::parse_ws_pcdata | pugi::parse_fragment))
            for(auto c : frag.children())
                if(c.type() == pugi::node_element)
                    parent.append_copy(c);
    }

    // ---- inline ----

    void write_inlines(
        pugi::xml_node parent, const std::vector<inline_node> &kids, const run_format &rf)
    {
        for(const auto &k : kids)
            std::visit([&](const auto &n) { emit_inline(parent, n, rf); }, k.v);
    }

    pugi::xml_node add_run(pugi::xml_node parent, const run_format &rf)
    {
        auto r = parent.append_child("w:r");
        std::string style_id = rf.style;
        if(style_id.empty() && rf.in_link)
            style_id = "Hyperlink";
        if(is_direct(style_id))
        {
            const auto &d = m_st.direct.at(style_id);
            pugi::xml_document frag;
            if(frag.load_buffer(
                   d.xml.data(), d.xml.size(), pugi::parse_default | pugi::parse_ws_pcdata))
                r.append_copy(frag.document_element());
            if(const std::string parent_id = d.def.m_parent_style.get_name(); !parent_id.empty())
                m_used_ids.emplace(char_id(parent_id), "character");
        }
        else if(!style_id.empty())
        {
            style_id = char_id(style_id);
            set_val(r.append_child("w:rPr").append_child("w:rStyle"), style_id);
            m_used_ids.emplace(style_id, "character");
        }
        return r;
    }

    void emit_text(pugi::xml_node parent, const std::string &s, const run_format &rf)
    {
        if(s.empty())
            return;
        auto r = add_run(parent, rf);
        std::string run;
        auto flush = [&]
        {
            if(run.empty())
                return;
            auto t = r.append_child("w:t");
            if(run.front() == ' ' || run.back() == ' ' || run.find("  ") != std::string::npos)
                t.append_attribute("xml:space") = "preserve";
            t.append_child(pugi::node_pcdata).set_value(run.c_str());
            run.clear();
        };
        for(char c : s)
        {
            if(c == '\t')
            {
                flush();
                r.append_child("w:tab");
            }
            else if(c == '\n')
            {
                flush();
                r.append_child("w:br");
            }
            else if(c != '\r')
                run += c;
        }
        flush();
    }

    void emit_inline(pugi::xml_node parent, const text &t, const run_format &rf)
    { emit_text(parent, t.content, rf); }

    void emit_inline(pugi::xml_node parent, const span &s, const run_format &rf)
    {
        run_format inner = rf;
        if(!s.style.is_empty())
            inner.style = s.style.get_name(); // runs don't nest: the innermost style wins
        write_inlines(parent, s.children(), inner);
    }

    void emit_inline(pugi::xml_node parent, const hyperlink &h, const run_format &rf)
    {
        auto n = parent.append_child("w:hyperlink");
        std::visit(
            overloaded{
                [&](const url_target &u)
                {
                    n.append_attribute("r:id") =
                        m_pkg.ensure_rel(m_part, rel::hyperlink, u.url, true).c_str();
                },
                [&](const bookmark_target &b) { n.append_attribute("w:anchor") = b.name.c_str(); },
            },
            h.target);
        n.append_attribute("w:history") = "1";
        run_format inner = rf;
        inner.in_link = true;
        write_inlines(n, h.children(), inner);
    }

    void emit_inline(pugi::xml_node parent, const frame &f, const run_format &rf)
    { write_drawing(add_run(parent, rf), f.img); }

    int bookmark_start(pugi::xml_node parent, const std::string &name)
    {
        const int id = m_bookmark_id++;
        auto n = parent.append_child("w:bookmarkStart");
        n.append_attribute("w:id") = id;
        n.append_attribute("w:name") = name.c_str();
        return id;
    }

    void emit_inline(pugi::xml_node parent, const bookmark &b, const run_format &)
    {
        const int id = bookmark_start(parent, b.name);
        parent.append_child("w:bookmarkEnd").append_attribute("w:id") = id;
    }

    // The field's result is what readers show until they update it: 1 for page
    // numbers, as pages aren't laid out here.
    void emit_inline(pugi::xml_node parent, const field &f, const run_format &rf)
    {
        std::string instr, value = "1";
        switch(f.type)
        {
        case field_type::page_number: instr = " PAGE "; break;
        case field_type::page_count: instr = " NUMPAGES "; break;
        case field_type::date:
            instr = " DATE \\@ \"yyyy-MM-dd\" ";
            value = detail::today();
            break;
        case field_type::title:
            instr = " TITLE ";
            value = m_doc.meta.title;
            break;
        }
        auto n = parent.append_child("w:fldSimple");
        n.append_attribute("w:instr") = instr.c_str();
        if(value.empty())
            add_run(n, rf);
        else
            emit_text(n, value, rf);
    }

    void emit_inline(pugi::xml_node parent, const unknown &u, const run_format &)
    { append_unknown(parent, u); }

    // ---- images ----

    /// The part an image is in, adding it to the package if needed: a document
    /// resource under its own name if that is in word/media/, else under
    /// word/media/; a file on disk likewise.
    std::string image_part(const std::string &uri)
    {
        if(auto it = m_image_parts.find(uri); it != m_image_parts.end())
            return it->second;

        std::string data, media_type;
        if(const resource *r = m_doc.resources().find(uri))
        {
            data = r->data();
            media_type = r->media_type().empty() ? media_type_of(uri) : r->media_type();
        }
        else if(m_pkg.contains(uri))
            return m_image_parts[uri] = uri;
        else if(fs::is_regular_file(uri))
        {
            data = detail::read_bytes(uri);
            media_type = media_type_of(uri);
        }
        else
            throw std::runtime_error("Image not found in the resources or on disk: " + uri);

        std::string name = uri;
        if(!name.starts_with("word/media/"))
        {
            const fs::path file = fs::path(uri).filename();
            name = "word/media/" + file.generic_string();
            for(int i = 2; m_pkg.contains(name) && m_pkg.find(name)->data != data; ++i)
                name = "word/media/" + file.stem().string() + "_" + std::to_string(i) +
                    file.extension().string();
        }
        if(const auto *p = m_pkg.find(name); !p || p->data != data)
            m_pkg.set(name, std::move(data), media_type);
        return m_image_parts[uri] = name;
    }

    void write_drawing(pugi::xml_node run, const image &img)
    {
        const std::string name = image_part(img.uri);
        const std::string id =
            m_pkg.ensure_rel(m_part, rel::image, opc::package::relative(m_part, name));
        const int drawing_id = m_drawing_id++;

        if(auto it = m_st.drawings.find(img.uri);
            it != m_st.drawings.end() && it->second.part == m_part)
        {
            pugi::xml_document frag;
            if(frag.load_buffer(it->second.xml.data(),
                   it->second.xml.size(),
                   pugi::parse_default | pugi::parse_ws_pcdata))
            {
                auto d = run.append_copy(frag.document_element());
                if(auto blip = find_descendant(d, "a:blip"))
                    blip.attribute("r:embed").set_value(id.c_str());
                if(auto pr = find_descendant(d, "wp:docPr"))
                    pr.attribute("id").set_value(drawing_id);
                return;
            }
        }

        // The image's own size, scaled down to the text width.
        long long cx = emu_per_inch, cy = emu_per_inch;
        if(const auto *p = m_pkg.find(name))
            if(auto sz = detail::read_image_size(p->data))
            {
                cx = std::llround(sz->width_inches() * emu_per_inch);
                cy = std::llround(sz->height_inches() * emu_per_inch);
            }
        if(cx > text_width_emu)
        {
            cy = cy * text_width_emu / cx;
            cx = text_width_emu;
        }
        const std::string file = fs::path(name).filename().string();
        const std::string ext =
            "cx=\"" + std::to_string(cx) + "\" cy=\"" + std::to_string(cy) + "\"";
        const std::string xml =
            "<w:drawing><wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\"><wp:extent " +
            ext + "/><wp:effectExtent l=\"0\" t=\"0\" r=\"0\" b=\"0\"/><wp:docPr id=\"" +
            std::to_string(drawing_id) + "\" name=\"Image " + std::to_string(drawing_id) +
            "\"/><wp:cNvGraphicFramePr><a:graphicFrameLocks "
            "noChangeAspect=\"1\"/></wp:cNvGraphicFramePr>"
            "<a:graphic><a:graphicData "
            "uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"><pic:pic>"
            "<pic:nvPicPr><pic:cNvPr id=\"0\" "
            "name=\"\"/><pic:cNvPicPr/></pic:nvPicPr><pic:blipFill><a:blip r:embed=\"" +
            id +
            "\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill><pic:spPr><a:xfrm><a:off "
            "x=\"0\" y=\"0\"/><a:ext " +
            ext +
            "/></a:xfrm><a:prstGeom "
            "prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr></pic:pic></a:graphicData>"
            "</a:graphic></wp:inline></w:drawing>";
        run.append_buffer(xml.data(), xml.size());
        find_descendant(run, "pic:cNvPr").attribute("name").set_value(file.c_str());
    }

    const text_doc &m_doc;
    state &m_st;
    opc::package &m_pkg;
    std::map<std::string, std::set<style_family>> m_used;
    std::set<std::string> m_dual; //!< Styles written as a paragraph and a character style
    std::set<std::pair<std::string, std::string>> m_used_ids; //!< styleId, w:type

    std::string m_main; //!< The main document part
    std::string m_part; //!< The part being written, whose relationships links and images get
    pugi::xml_document m_document, m_sect_pr, m_styles, m_numbering, m_settings;
    std::string m_styles_name, m_numbering_name, m_settings_name;
    std::map<std::string, part_xml> m_parts; //!< Other parts written, by name

    std::map<std::string, int> m_abstract_ids; //!< By list style name
    int m_next_abstract{0};
    int m_next_num{1};
    std::optional<list_context> m_list;
    int m_list_depth{0};
    int m_list_num{0};

    std::map<const heading *, std::string> m_toc_anchor;
    std::set<const heading *> m_toc_own;
    std::map<std::string, std::string> m_image_parts; //!< image uri -> part
    int m_bookmark_id{0};
    int m_drawing_id{1};
};
} // namespace

void write_document(const text_doc &doc, state &st) { writer(doc, st).write(); }

} // namespace docsmith::docx
