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
#include "docx/reader.h"
#include "docsmithcpp/error.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "docx/meta.h"
#include "docx/ooxml.h"
#include "docx/styles.h"
#include "outline.h"

namespace docsmith::docx
{
namespace
{
/// Builtin styles Word or LibreOffice may give another id, by display name.
const std::vector<std::pair<const char *, std::string>> &builtin_aliases()
{
    static const std::vector<std::pair<const char *, std::string>> aliases{
        {"Preformatted Text", builtin::preformatted.str()},
        {"HTML Preformatted", builtin::preformatted.str()},
        {"Source Text", builtin::source_text.str()},
        {"HTML Code", builtin::source_text.str()},
        {"Title", builtin::title.str()},
    };
    return aliases;
}

/// Run properties that alone don't make a run's formatting its own: the
/// language and proofing marks are on most runs Word writes.
bool is_incidental(std::string_view name)
{ return name == "w:lang" || name == "w:noProof" || name == "w:rStyle"; }

/// _Toc bookmarks, as Word and this writer name them (LibreOffice: _toc).
bool is_toc_bookmark(std::string_view name)
{
    if(name.size() < toc_bookmark_prefix.size())
        return false;
    for(std::size_t i = 0; i != toc_bookmark_prefix.size(); ++i)
        if(std::tolower(static_cast<unsigned char>(name[i])) !=
            std::tolower(static_cast<unsigned char>(toc_bookmark_prefix[i])))
            return false;
    return true;
}

std::string trim(std::string s)
{
    const auto b = s.find_first_not_of(' ');
    if(b == std::string::npos)
        return {};
    return s.substr(b, s.find_last_not_of(' ') - b + 1);
}

/// The instruction's first word, upper case: "PAGE" for " PAGE \* MERGEFORMAT ".
std::string field_name(const std::string &instr)
{
    std::istringstream in(instr);
    std::string word;
    in >> word;
    for(auto &c : word)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return word;
}

std::optional<field_type> field_of(const std::string &instr)
{
    const std::string name = field_name(instr);
    if(name == "PAGE")
        return field_type::page_number;
    if(name == "NUMPAGES")
        return field_type::page_count;
    if(name == "DATE")
        return field_type::date;
    if(name == "TITLE")
        return field_type::title;
    return std::nullopt;
}

/// The deepest level a TOC field lists: \o "1-3" gives 3.
int toc_levels(const std::string &instr)
{
    const auto o = instr.find("\\o");
    if(o == std::string::npos)
        return 3;
    const auto dash = instr.find('-', o);
    if(dash == std::string::npos)
        return 3;
    return std::clamp(std::atoi(instr.c_str() + dash + 1), 1, 9);
}

void append_text(std::vector<inline_node> &out, const std::string &s)
{
    if(!out.empty())
        if(auto *t = std::get_if<text>(&out.back().v))
        {
            t->content += s;
            return;
        }
    out.push_back(inline_node{text{s}});
}

/// A piece of a paragraph's content, before runs of the same style are
/// grouped into spans.
struct piece
{
    std::string style;
    std::optional<inline_node> node; //!< nullopt for a page break
    std::string text;                //!< Text, when node is empty and it isn't a page break
    bool is_break{false};
};

using pieces = std::vector<piece>;

/// Pieces into inline nodes: runs of one style in a span, text joined.
std::vector<inline_node> group(const pieces &ps)
{
    std::vector<inline_node> out;
    span *current = nullptr;
    std::string current_style;
    for(const auto &p : ps)
    {
        std::vector<inline_node> *target = &out;
        if(!p.style.empty())
        {
            if(!current || current_style != p.style)
            {
                out.push_back(inline_node{span().set_style(p.style)});
                current = &std::get<span>(out.back().v);
                current_style = p.style;
            }
            target = &current->items();
        }
        else
            current = nullptr;
        if(p.node)
            target->push_back(*p.node);
        else
            append_text(*target, p.text);
    }
    return out;
}

/// The list a paragraph is in: its numbering instance and level.
struct numbering_ref
{
    std::string num_id;
    int ilvl{0};
};

/// A complex field (w:fldChar begin ... end) being read.
struct field_state
{
    int depth{0};
    std::string instr;
    std::string xml;   //!< The runs, verbatim, in case it stays unknown
    std::string style; //!< The first run's
};

class reader
{
public:
    reader(state &st, text_doc &doc) :
        m_st(st), m_pkg(st.pkg), m_doc(doc)
    {
    }

    void read()
    {
        const auto *main = m_pkg.find(m_st.main_part);
        if(!main || !load(m_document, main->data) || !m_document.child("w:document"))
            throw error(error_code::malformed, "Could not read " + m_st.main_part);
        read_styles();
        read_numbering();

        m_part = m_st.main_part;
        auto body = m_document.child("w:document").child("w:body");
        m_doc.body = read_blocks(body);
        read_page(body.child("w:sectPr"));
        m_doc.meta = read_meta(m_pkg);

        // The images move from the package into the document, so every format
        // can reach them. Those only unknown markup shows stay in the package.
        for(const frame *f : collect<frame>(m_doc))
            if(!m_doc.resources().find(f->img.uri))
                if(auto p = m_pkg.take(f->img.uri))
                    m_doc.resources().insert_or_assign(
                        resource(f->img.uri, std::move(p->data), std::move(p->content_type)));
    }

private:
    // ---- parts ----

    std::string related_part(const char *type) const
    {
        const auto *r = m_pkg.find_rel_by_type(m_st.main_part, type);
        return r ? opc::package::resolve(m_st.main_part, r->target) : std::string();
    }

    bool load_part(pugi::xml_document &xml, const std::string &name) const
    {
        const auto *p = name.empty() ? nullptr : m_pkg.find(name);
        return p && load(xml, p->data);
    }

    // ---- styles ----

    void read_styles()
    {
        if(!load_part(m_styles, related_part(rel::styles)))
            return;
        auto root = m_styles.child("w:styles");
        m_defaults_ppr = root.child("w:docDefaults").child("w:pPrDefault").child("w:pPr");
        m_defaults_rpr = root.child("w:docDefaults").child("w:rPrDefault").child("w:rPr");
        // The model names a style as Word shows it, so the same name finds a
        // template's style in every format.
        std::set<std::string> names;
        for(auto s : root.children("w:style"))
        {
            const std::string id = s.attribute("w:styleId").as_string();
            m_names[id] = shown_name(id, val(s, "w:name"));
            names.insert(m_names[id]);
        }
        auto alias = [&](const std::string &id, const std::string &builtin)
        {
            if(m_names[id] != builtin && !names.contains(builtin) &&
                !m_alias_used.contains(builtin) && !m_alias.contains(id))
            {
                m_alias[id] = builtin;
                m_alias_used.insert(builtin);
            }
        };
        for(auto s : root.children("w:style"))
        {
            const std::string id = s.attribute("w:styleId").as_string();
            const std::string display = val(s, "w:name");
            const std::string_view type = s.attribute("w:type").as_string("paragraph");
            const std::string_view d = s.attribute("w:default").as_string();
            const bool is_default = d == "1" || d == "true" || d == "on";
            // The defaults by what they are, as their ids vary.
            if(is_default && type == "paragraph")
            {
                m_default_paragraph = id;
                alias(id, builtin::para.str());
            }
            else if(is_default && type == "table")
                alias(id, builtin::table.str());
            else if(type == "paragraph" && heading_level(id, display))
                alias(id, builtin::heading(heading_level(id, display)).str());
            else if(type == "character" &&
                (id == "Hyperlink" || display == "Hyperlink" || display == "Internet Link"))
                alias(id, builtin::link.str());
            for(const auto &[name, builtin] : builtin_aliases())
                if(display == name)
                    alias(id, builtin);
            m_parent[id] = val(s, "w:basedOn");
            m_style_nodes[id] = s;
            auto ppr = s.child("w:pPr");
            if(auto o = ppr.child("w:outlineLvl"))
                m_outline[id] = o.attribute("w:val").as_int();
            if(auto num = ppr.child("w:numPr"))
                m_style_num[id] =
                    numbering_ref{val(num, "w:numId"), std::atoi(val(num, "w:ilvl").c_str())};
        }
        for(auto s : root.children("w:style"))
            if(auto def = read_style(
                   s, name_of(s.attribute("w:styleId").as_string()), name_of(val(s, "w:basedOn"))))
                std::visit([&](const auto &d) { m_doc.styles().insert(d); }, *def);
    }

    /// The model's name for a style id.
    std::string name_of(const std::string &id) const
    {
        if(auto it = m_alias.find(id); it != m_alias.end())
            return it->second;
        if(auto it = m_names.find(id); it != m_names.end())
            return it->second;
        // Word's own ids, used without a definition.
        if(!m_style_nodes.contains(id))
        {
            if(const int level = heading_level(id, {}))
                return builtin::heading(level).str();
            if(id == "Hyperlink")
                return builtin::link.str();
        }
        return id;
    }

    /// A paragraph style's outline level (0-8), through its parents.
    std::optional<int> style_outline(std::string id) const
    {
        for(int guard = 0; !id.empty() && guard != 32; ++guard)
        {
            if(auto it = m_outline.find(id); it != m_outline.end())
                return it->second;
            auto p = m_parent.find(id);
            id = p != m_parent.end() ? p->second : std::string();
        }
        return std::nullopt;
    }

    std::optional<numbering_ref> style_numbering(std::string id) const
    {
        for(int guard = 0; !id.empty() && guard != 32; ++guard)
        {
            if(auto it = m_style_num.find(id); it != m_style_num.end())
                return it->second;
            auto p = m_parent.find(id);
            id = p != m_parent.end() ? p->second : std::string();
        }
        return std::nullopt;
    }

    /// A name for made-up styles not taken yet: T1, T2, ...
    std::string new_style_name(const char *prefix)
    {
        for(;;)
        {
            std::string name = prefix + std::to_string(++m_direct_count[prefix]);
            if(!m_doc.styles().para.contains(name) && !m_doc.styles().text.contains(name) &&
                !m_st.direct_para.contains(name) && !m_st.direct_text.contains(name))
                return name;
        }
    }

    /// The style standing for direct formatting: the same one for the same
    /// formatting on the same style.
    std::string direct_style(const std::string &parent, const text_props &tp, std::string xml)
    {
        const auto key = std::make_pair(parent, xml);
        if(auto it = m_direct_keys.find(key); it != m_direct_keys.end())
            return it->second;
        text_style def(new_style_name("T"), tp);
        def.parent = parent;
        const std::string name = def.name().str();
        m_doc.styles().insert(def);
        m_st.direct_text.insert_or_assign(name, direct_format<text_style>{def, std::move(xml)});
        return m_direct_keys[key] = name;
    }
    std::string direct_style(const std::string &parent, const paragraph_props &pp, std::string xml)
    {
        const auto key = std::make_pair(parent, xml);
        if(auto it = m_direct_keys.find(key); it != m_direct_keys.end())
            return it->second;
        paragraph_style def(new_style_name("P"), pp);
        def.parent = parent;
        const std::string name = def.name().str();
        m_doc.styles().insert(def);
        m_st.direct_para.insert_or_assign(
            name, direct_format<paragraph_style>{def, std::move(xml)});
        return m_direct_keys[key] = name;
    }

    /// The definition of a property that applies through these styles and
    /// their parents, else the document's default.
    pugi::xml_node inherited(
        const char *pr, std::string_view name, std::initializer_list<std::string> ids) const
    {
        for(std::string id : ids)
            for(int guard = 0; !id.empty() && guard != 32; ++guard)
            {
                auto it = m_style_nodes.find(id);
                if(it == m_style_nodes.end())
                    break;
                if(auto c = it->second.child(pr).child(name.data()))
                    return c;
                id = val(it->second, "w:basedOn");
            }
        return (std::string_view(pr) == "w:pPr" ? m_defaults_ppr : m_defaults_rpr)
            .child(name.data());
    }

    /// Whether a direct property says what applies anyway, as LibreOffice
    /// writes on every paragraph: the same as the style, or with nothing
    /// setting it, off or left aligned.
    bool redundant(
        const pugi::xml_node &prop, const char *pr, std::initializer_list<std::string> ids) const
    {
        if(auto def = inherited(pr, prop.name(), ids))
            return same(prop, def);
        if(std::string_view(prop.name()) == "w:jc")
        {
            const std::string_view v = prop.attribute("w:val").as_string();
            return v == "left" || v == "start";
        }
        return prop.attribute("w:val") && !prop.attribute("w:val").next_attribute() &&
            !prop.attribute("w:val").previous_attribute() && !prop.first_child() && !on(prop);
    }

    static bool same(const pugi::xml_node &a, const pugi::xml_node &b)
    {
        if(std::string_view(a.name()) != b.name())
            return false;
        std::size_t n = 0;
        for(auto at : a.attributes())
        {
            ++n;
            if(auto bt = b.attribute(at.name()); !bt || std::string_view(at.value()) != bt.value())
                return false;
        }
        for(auto bt : b.attributes())
            --n, (void)bt;
        if(n != 0)
            return false;
        std::string ca, cb;
        for(auto c : a.children())
            ca += xml::serialize(c);
        for(auto c : b.children())
            cb += xml::serialize(c);
        return ca == cb;
    }

    /// The paragraph style that applies: its own, else the default one.
    std::string paragraph_style_id(const std::string &id) const
    { return id.empty() ? m_default_paragraph : id; }

    /// A run's style: its w:rStyle, or a made-up one for direct formatting.
    std::string run_style(const pugi::xml_node &rpr, bool in_link)
    {
        const std::string rstyle = val(rpr, "w:rStyle");
        std::string id = rstyle;
        if(in_link && name_of(id) == builtin::link.str())
            id.clear();
        pugi::xml_document own;
        auto copy = own.append_copy(rpr);
        bool significant = false;
        for(auto c = copy.first_child(); c;)
        {
            auto next = c.next_sibling();
            if(c.type() == pugi::node_element && std::string_view(c.name()) != "w:rStyle")
            {
                if(redundant(c, "w:rPr", {rstyle, paragraph_style_id(m_paragraph_style)}))
                    copy.remove_child(c);
                else if(!is_incidental(c.name()))
                    significant = true;
            }
            c = next;
        }
        if(!significant)
            return name_of(id);
        return direct_style(
            name_of(rstyle), read_rpr(copy).value_or(text_props{}), xml::serialize(copy));
    }

    // ---- numbering ----

    void read_numbering()
    {
        if(!load_part(m_numbering, related_part(rel::numbering)))
            return;
        auto root = m_numbering.child("w:numbering");
        std::map<std::string, pugi::xml_node> abstracts;
        for(auto a : root.children("w:abstractNum"))
            abstracts[a.attribute("w:abstractNumId").as_string()] = a;
        for(auto n : root.children("w:num"))
            if(auto it = abstracts.find(val(n, "w:abstractNumId")); it != abstracts.end())
            {
                m_nums[n.attribute("w:numId").as_string()] = it->second;
                m_num_nodes[n.attribute("w:numId").as_string()] = n;
            }
    }

    /// The count a numbering instance restarts at, at a level, if it does.
    std::optional<int> start_override(const std::string &num_id, int ilvl) const
    {
        auto it = m_num_nodes.find(num_id);
        if(it == m_num_nodes.end())
            return std::nullopt;
        for(auto o : it->second.children("w:lvlOverride"))
            if(o.attribute("w:ilvl").as_int() == ilvl)
                if(auto s = o.child("w:startOverride"))
                    return s.attribute("w:val").as_int();
        return std::nullopt;
    }

    /// A level of an instance's definition, if it is numbered (not bullets).
    pugi::xml_node numbered_level(const std::string &num_id, int ilvl) const
    {
        auto it = m_nums.find(num_id);
        if(it == m_nums.end())
            return {};
        for(auto lvl : it->second.children("w:lvl"))
            if(lvl.attribute("w:ilvl").as_int() == ilvl)
            {
                const std::string fmt = val(lvl, "w:numFmt");
                return fmt == "bullet" || fmt == "none" ? pugi::xml_node{} : lvl;
            }
        return {};
    }

    bool same_numbering(const std::string &a, const std::string &b) const
    {
        auto ia = m_nums.find(a), ib = m_nums.find(b);
        return ia != m_nums.end() && ib != m_nums.end() && ia->second == ib->second;
    }

    /// The list style of a numbering instance, added to the registry the first
    /// time. The writer's default list reads back unstyled.
    std::string list_style_of(const std::string &num_id)
    {
        auto it = m_nums.find(num_id);
        if(it == m_nums.end())
            return {};
        const auto a = it->second;
        std::string name = val(a, "w:name");
        if(name == default_list_name)
            return {};
        if(name.empty())
            name = "WWNum" + std::string(a.attribute("w:abstractNumId").as_string());
        if(!m_doc.styles().list.contains(name))
            m_doc.styles().insert(read_abstract_num(a, name));
        return name == builtin::list.str() ? std::string() : name;
    }

    // ---- page ----

    void read_page(const pugi::xml_node &sect)
    {
        auto read = [&](const char *ref, std::vector<block_node> &out)
        {
            for(auto r : sect.children(ref))
            {
                if(std::string_view(r.attribute("w:type").as_string("default")) != "default")
                    continue;
                const auto *rel = m_pkg.find_rel(m_st.main_part, r.attribute("r:id").as_string());
                pugi::xml_document xml;
                if(!rel || !load_part(xml, opc::package::resolve(m_st.main_part, rel->target)))
                    continue;
                m_part = opc::package::resolve(m_st.main_part, rel->target);
                out = read_blocks(xml.document_element());
                m_part = m_st.main_part;
            }
        };
        read("w:headerReference", m_doc.page.header);
        read("w:footerReference", m_doc.page.footer);
    }

    // ---- blocks ----

    /// Paragraphs in lists are gathered into list nodes as they're read.
    struct open_list
    {
        list l;
        int ilvl;
        std::string num_id;
    };

    void close_list(std::vector<open_list> &stack, std::vector<block_node> &out)
    {
        open_list top = std::move(stack.back());
        stack.pop_back();
        if(stack.empty())
            out.push_back(block_node{std::move(top.l)});
        else
        {
            auto &items = stack.back().l.items();
            if(items.empty())
                items.emplace_back();
            items.back().items().push_back(block_node{std::move(top.l)});
        }
    }

    void close_lists(std::vector<open_list> &stack, std::vector<block_node> &out)
    {
        while(!stack.empty())
            close_list(stack, out);
    }

    std::vector<block_node> read_blocks(const pugi::xml_node &container)
    {
        std::vector<block_node> out;
        std::vector<open_list> stack;
        std::set<std::string> dropped_bookmarks;

        for(auto c = container.first_child(); c; c = c.next_sibling())
        {
            if(c.type() != pugi::node_element)
                continue;
            const std::string_view name = c.name();
            if(name == "w:p")
            {
                if(is_toc_start(c))
                {
                    close_lists(stack, out);
                    c = read_toc_field(c, out);
                    if(!c)
                        break;
                    continue;
                }
                read_paragraph(c, stack, out);
                continue;
            }
            if(name == "w:sectPr" || name == "w:tcPr")
                continue;
            if(name == "w:bookmarkStart" || name == "w:bookmarkEnd")
            {
                const std::string id = c.attribute("w:id").as_string();
                if(name == "w:bookmarkStart" && is_toc_bookmark(c.attribute("w:name").as_string()))
                {
                    dropped_bookmarks.insert(id);
                    continue;
                }
                if(name == "w:bookmarkEnd" && dropped_bookmarks.contains(id))
                    continue;
            }
            close_lists(stack, out);
            if(name == "w:tbl")
                out.push_back(block_node{read_table(c)});
            else if(name == "w:sdt" && is_toc_sdt(c))
                out.push_back(block_node{read_toc_sdt(c)});
            else
                out.push_back(block_node{unknown{xml::serialize(c), markup::ooxml}});
        }
        close_lists(stack, out);
        return out;
    }

    void read_paragraph(
        const pugi::xml_node &p, std::vector<open_list> &stack, std::vector<block_node> &out)
    {
        auto ppr = p.child("w:pPr");
        const std::string style_id = val(ppr, "w:pStyle");

        std::optional<int> outline;
        if(auto o = ppr.child("w:outlineLvl"))
            outline = o.attribute("w:val").as_int();
        else
            outline = style_outline(style_id);
        if(outline && (*outline < 0 || *outline > 8))
            outline.reset(); // 9 is body text
        const int level = outline ? *outline + 1 : 0;

        // Direct formatting: what pPr says beyond what the model holds.
        // A paragraph with no style has the default, which direct formatting
        // then derives from.
        std::string own_style = name_of(paragraph_style_id(style_id));
        bool break_before = false;
        if(ppr)
        {
            pugi::xml_document own;
            auto copy = own.append_copy(ppr);
            for(const char *n : {"w:pStyle", "w:numPr", "w:outlineLvl"})
                copy.remove_child(n);
            // The indent the writer gives a later paragraph of a list item.
            if(auto num = ppr.child("w:numPr"); val(num, "w:numId") == "0")
                if(auto ind = copy.child("w:ind"); ind &&
                    !ind.attribute("w:left").next_attribute() &&
                    !ind.attribute("w:left").previous_attribute() &&
                    ind.attribute("w:left").as_int() ==
                        list_indent(std::atoi(val(num, "w:ilvl").c_str())))
                    copy.remove_child(ind);
            if(auto b = copy.child("w:pageBreakBefore"))
            {
                break_before = on(b);
                copy.remove_child(b);
            }
            bool significant = false;
            for(auto c = copy.first_child(); c;)
            {
                auto next = c.next_sibling();
                if(c.type() == pugi::node_element && std::string_view(c.name()) != "w:rPr")
                {
                    if(redundant(c, "w:pPr", {paragraph_style_id(style_id)}))
                        copy.remove_child(c);
                    else
                        significant = true;
                }
                c = next;
            }
            if(significant)
            {
                own_style = direct_style(
                    own_style, read_ppr(copy).value_or(paragraph_props{}), xml::serialize(copy));
            }
        }
        // The defaults read as no style: the default paragraph style, and a
        // heading's Heading<level>.
        if((level && own_style == builtin::heading(level).str()) ||
            (!m_default_paragraph.empty() && own_style == name_of(m_default_paragraph)))
            own_style.clear();

        // Lists: numbering set directly or by the style. numId 0 says none.
        std::optional<numbering_ref> num;
        bool continuation = false;
        if(auto n = ppr.child("w:numPr"))
        {
            num = numbering_ref{val(n, "w:numId"), std::atoi(val(n, "w:ilvl").c_str())};
            if(num->num_id.empty())
                if(auto s = style_numbering(style_id))
                    num->num_id = s->num_id;
        }
        else if(auto s = style_numbering(style_id))
            num = s;
        if(num && (num->num_id == "0" || num->num_id.empty()))
        {
            continuation = !stack.empty();
            num.reset();
        }
        if(level)
            num.reset();

        // The content, split at page breaks.
        pieces ps;
        m_paragraph_style = style_id;
        read_content(p, ps, false);
        std::vector<block_node> blocks;
        if(break_before)
            blocks.push_back(block_node{page_break{}});
        std::vector<pieces> segments(1);
        bool had_break = false;
        for(auto &pc : ps)
        {
            if(pc.is_break)
            {
                segments.emplace_back();
                had_break = true;
            }
            else
                segments.back().push_back(std::move(pc));
        }
        for(std::size_t i = 0; i != segments.size(); ++i)
        {
            if(i != 0)
                blocks.push_back(block_node{page_break{}});
            if(had_break && segments[i].empty())
                continue;
            auto kids = group(segments[i]);
            if(level)
            {
                heading h(level);
                h.style = own_style;
                h.items() = std::move(kids);
                blocks.push_back(block_node{std::move(h)});
            }
            else if(own_style.empty() && kids.size() == 1 &&
                std::holds_alternative<frame>(kids.front().v))
                blocks.push_back(block_node{std::get<frame>(kids.front().v)});
            else
            {
                paragraph para;
                para.style = own_style;
                para.items() = std::move(kids);
                blocks.push_back(block_node{std::move(para)});
            }
        }

        if(num)
        {
            add_list_item(*num, std::move(blocks), stack, out);
            return;
        }
        if(continuation)
        {
            const int ilvl = std::atoi(val(ppr.child("w:numPr"), "w:ilvl").c_str());
            while(stack.size() > 1 && stack.back().ilvl > ilvl)
                close_list(stack, out);
            // Deeper than any open list: an unnumbered item at the start of
            // a nested list, as the writer writes one. The list takes its
            // numbering from its first numbered item.
            if(stack.back().ilvl < ilvl)
            {
                open_list pending{list(), ilvl, {}};
                list_item item;
                item.numbered = false;
                item.items() = std::move(blocks);
                pending.l.add(std::move(item));
                stack.push_back(std::move(pending));
                return;
            }
            auto &items = stack.back().l.items();
            if(items.empty())
                items.emplace_back();
            for(auto &b : blocks)
                items.back().items().push_back(std::move(b));
            return;
        }
        close_lists(stack, out);
        for(auto &b : blocks)
            out.push_back(std::move(b));
    }

    void add_list_item(const numbering_ref &num, std::vector<block_node> blocks,
        std::vector<open_list> &stack, std::vector<block_node> &out)
    {
        // Word counts on through every instance (w:num) of a numbering, from
        // the first, unless one restarts the count at a level the first time
        // it's used there.
        const bool first_use = m_used_nums.insert({num.num_id, num.ilvl}).second;
        const auto restart = first_use ? start_override(num.num_id, num.ilvl) : std::nullopt;
        const auto lvl = numbered_level(num.num_id, num.ilvl);
        const auto abstract =
            m_nums.contains(num.num_id) ? m_nums.at(num.num_id) : pugi::xml_node{};
        const bool counted_before = !m_used_abstracts.insert({abstract, num.ilvl}).second;

        list_item item;
        while(!stack.empty() && stack.back().ilvl > num.ilvl)
            close_list(stack, out);
        // A list opened by unnumbered items, waiting for its numbering.
        std::vector<list_item> unnumbered;
        if(!stack.empty() && stack.back().ilvl == num.ilvl && stack.back().num_id.empty())
        {
            unnumbered = std::move(stack.back().l.items());
            stack.pop_back();
        }
        const auto own_start = [&] { return lvl.child("w:start").attribute("w:val").as_int(1); };
        if(!stack.empty() && stack.back().ilvl == num.ilvl && stack.back().num_id != num.num_id)
        {
            // Another instance of the same numbering carries on the list,
            // unless it starts again from the numbering's start: then it's a
            // new list of the same style, as the writer writes one.
            if(same_numbering(stack.back().num_id, num.num_id) &&
                !(lvl && restart && *restart == own_start()))
            {
                stack.back().num_id = num.num_id;
                if(lvl)
                    item.start_value = restart;
            }
            else
                close_list(stack, out);
        }
        if(stack.empty() || stack.back().ilvl < num.ilvl)
        {
            // A nested list continuing its parent's numbering is unstyled.
            const bool same = !stack.empty() && stack.back().num_id == num.num_id;
            open_list l{list(), num.ilvl, num.num_id};
            if(!same)
            {
                l.l.set_style(list_style_of(num.num_id));
                // A level restarts after each item of a level above in the
                // same numbering, so a nested list continues only when its
                // numbering isn't its parent's.
                l.l.continue_numbering = lvl &&
                    (stack.empty() || !same_numbering(stack.back().num_id, num.num_id)) &&
                    (first_use ? counted_before && !restart : true);
                // A restart at the numbering's own start is just a new list.
                if(lvl && restart && *restart != own_start())
                    item.start_value = restart;
            }
            for(auto &u : unnumbered)
                l.l.add(std::move(u));
            stack.push_back(std::move(l));
        }
        else
            for(auto &u : unnumbered) // the numbering's list was open already
                stack.back().l.add(std::move(u));
        item.items() = std::move(blocks);
        stack.back().l.add(std::move(item));
    }

    // ---- inline ----

    void add_node(pieces &out, const std::string &style, inline_node n)
    { out.push_back(piece{style, std::move(n)}); }

    void add_text(pieces &out, const std::string &style, std::string s)
    {
        if(!out.empty() && !out.back().node && !out.back().is_break && out.back().style == style)
            out.back().text += s;
        else
            out.push_back(piece{style, std::nullopt, std::move(s)});
    }

    void read_content(const pugi::xml_node &container, pieces &out, bool in_link)
    {
        field_state fld;
        for(auto c : container.children())
        {
            if(c.type() != pugi::node_element)
                continue;
            const std::string_view name = c.name();
            if(name == "w:pPr" || name == "w:proofErr" || name == "w:bookmarkEnd")
                continue;
            if(fld.depth > 0 && name != "w:r")
            {
                fld.xml += xml::serialize(c);
                continue;
            }
            if(name == "w:r")
                read_run(c, out, in_link, fld);
            else if(name == "w:hyperlink")
                read_hyperlink(c, out);
            else if(name == "w:fldSimple")
            {
                pieces inner;
                read_content(c, inner, in_link);
                const std::string style = inner.empty() ? std::string() : inner.front().style;
                if(auto f = field_of(c.attribute("w:instr").as_string()))
                    add_node(out, style, inline_node{field{*f}});
                else
                    add_node(out, "", inline_node{unknown{xml::serialize(c), markup::ooxml}});
            }
            else if(name == "w:bookmarkStart")
            {
                const std::string b = c.attribute("w:name").as_string();
                if(!is_toc_bookmark(b))
                    add_node(out, "", inline_node{bookmark{b}});
            }
            else if(name == "w:smartTag" || name == "w:customXml" || name == "w:dir" ||
                name == "w:bdo")
                read_content(c, out, in_link); // their content, without the wrapper
            else
                add_node(out, "", inline_node{unknown{xml::serialize(c), markup::ooxml}});
        }
        if(fld.depth > 0) // a field that goes on into the next paragraph
            add_node(out, "", inline_node{unknown{fld.xml, markup::ooxml}});
    }

    void read_hyperlink(const pugi::xml_node &h, pieces &out)
    {
        hyperlink link;
        const std::string anchor = h.attribute("w:anchor").as_string();
        if(const auto *r = m_pkg.find_rel(m_part, h.attribute("r:id").as_string()))
            link.target = url_target{r->target + (anchor.empty() ? "" : "#" + anchor)};
        else
            link.target = bookmark_target{anchor};
        pieces inner;
        read_content(h, inner, true);
        link.items() = group(inner);
        // Text all in one style (other than Hyperlink, read as none) is the
        // link's style, as the writer writes it.
        auto &kids = link.items();
        const auto *first = kids.empty() ? nullptr : std::get_if<span>(&kids.front().v);
        if(first && !first->style.empty() &&
            std::all_of(kids.begin(),
                kids.end(),
                [&](const inline_node &k)
                {
                    const auto *s = std::get_if<span>(&k.v);
                    return s && s->style == first->style;
                }))
        {
            link.style = first->style;
            std::vector<inline_node> unwrapped;
            for(auto &k : kids)
                for(auto &c : std::get<span>(k.v).items())
                    unwrapped.push_back(std::move(c));
            kids = std::move(unwrapped);
        }
        add_node(out, "", inline_node{std::move(link)});
    }

    /// A run's content element as an unknown node: a run of its own, keeping
    /// the formatting.
    static inline_node unknown_run(const pugi::xml_node &rpr, const pugi::xml_node &child)
    {
        pugi::xml_document d;
        auto r = d.append_child("w:r");
        if(rpr)
            r.append_copy(rpr);
        r.append_copy(child);
        return inline_node{unknown{xml::serialize(r), markup::ooxml}};
    }

    void read_run(const pugi::xml_node &r, pieces &out, bool in_link, field_state &fld)
    {
        const auto rpr = r.child("w:rPr");
        const bool has_field_char = r.child("w:fldChar") || r.child("w:instrText");
        if(fld.depth > 0 || has_field_char)
        {
            read_field_run(r, out, in_link, fld);
            return;
        }
        const std::string style = run_style(rpr, in_link);
        for(auto c : r.children())
        {
            if(c.type() != pugi::node_element)
                continue;
            const std::string_view name = c.name();
            if(name == "w:rPr" || name == "w:lastRenderedPageBreak")
                continue;
            if(name == "w:t")
                add_text(out, style, c.text().get());
            else if(name == "w:tab" || name == "w:ptab")
                add_text(out, style, "\t");
            else if(name == "w:cr")
                add_text(out, style, "\n");
            else if(name == "w:br")
            {
                const std::string_view type = c.attribute("w:type").as_string("textWrapping");
                if(type == "page")
                    out.push_back(piece{"", std::nullopt, {}, true});
                else if(type == "textWrapping")
                    add_text(out, style, "\n");
                else
                    add_node(out, "", unknown_run(rpr, c));
            }
            else if(name == "w:noBreakHyphen")
                add_text(out, style, "‑");
            else if(name == "w:softHyphen")
                add_text(out, style, "­");
            else if(name == "w:drawing")
            {
                if(auto f = read_drawing(c))
                    add_node(out, style, inline_node{std::move(*f)});
                else
                    add_node(out, "", unknown_run(rpr, c));
            }
            else
                add_node(out, "", unknown_run(rpr, c));
        }
    }

    /// Runs of a complex field: a known field becomes a field node, any other
    /// stays unknown, all its runs in one node.
    void read_field_run(const pugi::xml_node &r, pieces &out, bool in_link, field_state &fld)
    {
        if(fld.depth == 0)
        {
            fld = field_state{};
            fld.style = run_style(r.child("w:rPr"), in_link);
        }
        fld.xml += xml::serialize(r);
        for(auto c : r.children())
        {
            const std::string_view name = c.name();
            if(name == "w:instrText" && fld.depth == 1)
                fld.instr += c.text().get();
            else if(name == "w:fldChar")
            {
                const std::string_view type = c.attribute("w:fldCharType").as_string();
                if(type == "begin")
                    ++fld.depth;
                else if(type == "end")
                    --fld.depth;
            }
        }
        if(fld.depth > 0)
            return;
        if(auto f = field_of(fld.instr))
            add_node(out, fld.style, inline_node{field{*f}});
        else
            add_node(out, "", inline_node{unknown{fld.xml, markup::ooxml}});
        fld = field_state{};
    }

    /// A w:drawing as XML to write back, with a floating picture (wp:anchor)
    /// made inline: the model doesn't hold anchors or positions (DESIGN.md,
    /// Rules). wp:inline takes the anchor's size, alt text and picture,
    /// which come in the same order.
    static std::string inline_drawing(const pugi::xml_node &d)
    {
        pugi::xml_document copy;
        auto root = copy.append_copy(d);
        if(auto a = root.child("wp:anchor"))
        {
            auto in = root.insert_child_before("wp:inline", a);
            for(const char *dist : {"distT", "distB", "distL", "distR"})
                in.append_attribute(dist) = 0;
            for(auto c : a.children())
                for(const char *keep :
                    {"wp:extent", "wp:effectExtent", "wp:docPr", "wp:cNvGraphicFramePr", "a:graphic"})
                    if(std::string_view(c.name()) == keep)
                        in.append_copy(c);
            root.remove_child(a);
        }
        return xml::serialize(root);
    }

    std::optional<frame> read_drawing(const pugi::xml_node &d)
    {
        pugi::xml_node blip;
        auto find = [&](auto &&self, const pugi::xml_node &n) -> void
        {
            for(auto c : n.children())
            {
                if(blip)
                    return;
                if(std::string_view(c.name()) == "a:blip" &&
                    c.parent().parent().name() == std::string_view("pic:pic"))
                    blip = c;
                else
                    self(self, c);
            }
        };
        find(find, d);
        if(!blip)
            return std::nullopt;
        const auto *r = m_pkg.find_rel(m_part, blip.attribute("r:embed").as_string());
        if(!r || r->external)
            return std::nullopt;
        const std::string uri = opc::package::resolve(m_part, r->target);
        if(!m_pkg.contains(uri) && !m_doc.resources().find(uri))
            return std::nullopt;
        if(!m_st.drawings.contains(uri))
            m_st.drawings[uri] = drawing{m_part, inline_drawing(d)};
        frame f(image{uri});
        for(auto shape : d.children())
            if(auto pr = shape.child("wp:docPr"))
            {
                f.accessibility_title = pr.attribute("title").as_string();
                f.accessibility_description = pr.attribute("descr").as_string();
            }
        for(auto shape : d.children())
            if(auto ext = shape.child("wp:extent"))
            {
                // EMU: 12700 to the point.
                f.width = pt(ext.attribute("cx").as_double() / 12700);
                f.height = pt(ext.attribute("cy").as_double() / 12700);
            }
        return f;
    }

    // ---- tables ----

    table read_table(const pugi::xml_node &tbl)
    {
        std::vector<std::vector<table_cell>> rows;
        std::size_t header_rows = 0;
        // Where each slot's cell is, to extend it when a row merges into it.
        std::vector<std::vector<std::pair<std::size_t, std::size_t>>> origin;
        bool header = true;
        for(auto tr : tbl.children("w:tr"))
        {
            const std::size_t r = rows.size();
            rows.emplace_back();
            origin.emplace_back();
            auto &row = rows.back();
            auto pad = [&](int n)
            {
                for(int i = 0; i < n; ++i)
                {
                    origin.back().emplace_back(r, row.size());
                    row.emplace_back();
                }
            };
            auto tr_pr = tr.child("w:trPr");
            if(header && tr_pr.child("w:tblHeader") && on(tr_pr.child("w:tblHeader")))
                header_rows = r + 1;
            else
                header = false;
            pad(std::atoi(val(tr_pr, "w:gridBefore").c_str()));

            for(auto tc : tr.children("w:tc"))
            {
                auto tc_pr = tc.child("w:tcPr");
                const int span = std::max(1, std::atoi(val(tc_pr, "w:gridSpan").c_str()));
                const std::size_t c = row.size();
                auto vm = tc_pr.child("w:vMerge");
                const bool merged = vm &&
                    std::string_view(vm.attribute("w:val").as_string("continue")) != "restart";
                if(merged && r > 0 && c < origin[r - 1].size())
                {
                    const auto o = origin[r - 1][c];
                    auto &cell = rows[o.first][o.second];
                    cell.rowspan = static_cast<int>(r - o.first) + 1;
                    // The merged cell's last row has its bottom edge.
                    if(auto b = read_tcpr(tc_pr).m_borders.m_bottom)
                        cell.props.m_borders.m_bottom = b;
                    for(int i = 0; i < span; ++i)
                    {
                        origin.back().push_back(o);
                        row.emplace_back();
                    }
                    continue;
                }
                table_cell cell;
                cell.colspan = span;
                cell.props = read_tcpr(tc_pr);
                cell.items() = read_blocks(tc);
                // The writer ends every cell with a paragraph.
                auto &kids = cell.items();
                auto empty_paragraph = [](const block_node &b)
                {
                    const auto *p = std::get_if<paragraph>(&b.v);
                    return p && p->style.empty() && p->items().empty();
                };
                if(!kids.empty() && empty_paragraph(kids.back()) &&
                    (kids.size() == 1 || std::holds_alternative<table>(kids[kids.size() - 2].v)))
                    kids.pop_back();
                origin.back().emplace_back(r, c);
                row.push_back(std::move(cell));
                for(int i = 1; i < span; ++i)
                {
                    origin.back().emplace_back(r, c);
                    row.emplace_back();
                }
            }
            pad(std::atoi(val(tr_pr, "w:gridAfter").c_str()));
        }
        std::size_t cols = 0;
        for(const auto &row : rows)
            cols = std::max(cols, row.size());
        table t;
        for(auto &row : rows)
        {
            row.resize(cols);
            t.add_row(std::move(row));
        }
        t.set_header_rows(header_rows);
        auto tbl_pr = tbl.child("w:tblPr");
        if(const std::string id = val(tbl_pr, "w:tblStyle"); !id.empty())
            if(name_of(id) != builtin::table.str())
                t.style = name_of(id);
        t.accessibility_title = val(tbl_pr, "w:tblCaption");
        t.accessibility_description = val(tbl_pr, "w:tblDescription");

        std::vector<double> widths;
        for(auto col : tbl.child("w:tblGrid").children("w:gridCol"))
            widths.push_back(col.attribute("w:w").as_double());
        if(widths.size() == cols &&
            std::any_of(
                widths.begin(), widths.end(), [&](double w) { return w != widths.front(); }) &&
            std::all_of(widths.begin(), widths.end(), [](double w) { return w > 0; }))
            t.set_column_widths(widths);
        return t;
    }

    // ---- table of contents ----

    static bool is_toc_sdt(const pugi::xml_node &sdt)
    {
        return val(sdt.child("w:sdtPr").child("w:docPartObj"), "w:docPartGallery") ==
            "Table of Contents";
    }

    /// The instruction of the first complex field in a paragraph.
    static std::string first_instr(const pugi::xml_node &p)
    {
        std::string instr;
        bool begun = false;
        for(auto r : p.children("w:r"))
            for(auto c : r.children())
            {
                const std::string_view name = c.name();
                if(name == "w:fldChar")
                {
                    const std::string_view type = c.attribute("w:fldCharType").as_string();
                    if(type == "begin" && !begun)
                        begun = true;
                    else if(begun)
                        return instr;
                }
                else if(name == "w:instrText" && begun)
                    instr += c.text().get();
            }
        return instr;
    }

    static bool is_toc_start(const pugi::xml_node &p)
    { return field_name(first_instr(p)) == "TOC"; }

    /// Reads a TOC field that runs over paragraphs from p to the one ending it,
    /// and returns that one. A title paragraph just before it goes with it.
    pugi::xml_node read_toc_field(pugi::xml_node p, std::vector<block_node> &out)
    {
        table_of_contents toc(std::string(), toc_levels(first_instr(p)));
        if(!out.empty())
            if(const auto *title = std::get_if<paragraph>(&out.back().v);
                title && title->style.str() == "TOCHeading")
            {
                toc.title = trim(detail::plain_text(title->items()));
                out.pop_back();
            }
        int depth = 0;
        for(; p; p = p.next_sibling())
        {
            for(auto fc : p.select_nodes(".//w:fldChar"))
            {
                const std::string_view type = fc.node().attribute("w:fldCharType").as_string();
                depth += type == "begin" ? 1 : type == "end" ? -1 : 0;
            }
            if(depth <= 0)
                break;
        }
        out.push_back(block_node{std::move(toc)});
        return p;
    }

    /// A table of contents in a content control: the paragraphs before the
    /// field are its title.
    table_of_contents read_toc_sdt(const pugi::xml_node &sdt)
    {
        table_of_contents toc(std::string(), 3);
        for(auto p : sdt.child("w:sdtContent").children("w:p"))
        {
            if(const std::string instr = first_instr(p); field_name(instr) == "TOC")
            {
                toc.levels = toc_levels(instr);
                break;
            }
            pieces ps;
            read_content(p, ps, false);
            toc.title += (toc.title.empty() ? "" : " ") + trim(detail::plain_text(group(ps)));
        }
        return toc;
    }

    state &m_st;
    opc::package &m_pkg;
    text_doc &m_doc;
    std::string m_part; //!< The part being read, whose relationships links and images use
    pugi::xml_document m_document, m_styles, m_numbering;

    std::map<std::string, std::string> m_alias; //!< style id -> builtin name
    std::map<std::string, std::string> m_names; //!< style id -> its shown name
    std::set<std::string> m_alias_used;
    std::map<std::string, std::string> m_parent; //!< style id -> w:basedOn
    std::map<std::string, pugi::xml_node> m_style_nodes;
    pugi::xml_node m_defaults_ppr, m_defaults_rpr;
    std::string m_default_paragraph;      //!< The style of paragraphs with none
    std::string m_paragraph_style;        //!< The style id of the paragraph being read
    std::map<std::string, int> m_outline; //!< style id -> outline level
    std::map<std::string, numbering_ref> m_style_num;
    std::map<std::string, pugi::xml_node> m_nums;              //!< numId -> abstractNum
    std::map<std::string, pugi::xml_node> m_num_nodes;         //!< numId -> w:num
    std::set<std::pair<std::string, int>> m_used_nums;         //!< numId, ilvl of items read
    std::set<std::pair<pugi::xml_node, int>> m_used_abstracts; //!< abstractNum, ilvl likewise
    std::map<std::string, int> m_direct_count;
    std::map<std::pair<std::string, std::string>, std::string> m_direct_keys;
};
} // namespace

void read_document(state &st, text_doc &doc) { reader(st, doc).read(); }

} // namespace docsmith::docx
