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
#include "docsmithcpp/html/file.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

#include <fmt/format.h>

// The page is written directly rather than built as an XML tree: HTML is not
// XML. An empty <p/> or <a id="x"/> is an *open* tag to an HTML parser, so
// non-void elements always get an end tag, and void ones (<img>, <meta>) never
// do. Inline content stays on one line, because whitespace between inline
// elements would show up as spaces.

namespace docsmith::html
{
namespace fs = std::filesystem;

namespace
{

void escape(std::ostream &os, std::string_view s, bool attribute = false)
{
    for(char c : s)
    {
        switch(c)
        {
        case '&': os << "&amp;"; break;
        case '<': os << "&lt;"; break;
        case '>': os << "&gt;"; break;
        case '"':
            if(attribute)
            {
                os << "&quot;";
                break;
            }
            [[fallthrough]];
        default: os << c;
        }
    }
}

/// A style name as a CSS class. ODF names such as "Text_20_body" are already
/// valid identifiers, but code-built names may contain spaces and the like.
std::string css_class(std::string_view name)
{
    std::string out;
    for(unsigned char c : name)
        out += (std::isalnum(c) || c == '-' || c == '_' || c >= 0x80) ? static_cast<char>(c) : '_';
    if(!out.empty() && std::isdigit(static_cast<unsigned char>(out.front())))
        out.insert(out.begin(), '_');
    return out;
}

/// An element id from a bookmark name; ids can't contain whitespace.
std::string html_id(std::string_view name)
{
    std::string out(name);
    std::replace_if(out.begin(), out.end(), [](unsigned char c) { return std::isspace(c); }, '_');
    return out;
}

const char *css_value(font_style fs)
{
    switch(fs)
    {
    case font_style::italic: return "italic";
    case font_style::oblique: return "oblique";
    default: return "normal";
    }
}

const char *css_value(break_type b)
{
    switch(b)
    {
    case break_type::column: return "column";
    case break_type::automatic: return "auto";
    default: return "page";
    }
}

/// CSS rules for the style definitions in the document, sorted by name so the
/// output is stable.
std::string generated_css(const text_doc &doc)
{
    std::vector<const style *> styles;
    for(const auto &[name, s] : doc.styles())
        styles.push_back(&s);
    std::sort(styles.begin(), styles.end(),
        [](const style *a, const style *b) { return a->m_name.get_name() < b->m_name.get_name(); });

    std::string css;
    for(const style *s : styles)
    {
        std::string decls;
        if(const auto &tp = s->m_text_props)
        {
            if(tp->m_font_size)
                decls += fmt::format(" font-size: {}pt;", tp->m_font_size->m_points);
            if(tp->m_font_style)
                decls += fmt::format(" font-style: {};", css_value(*tp->m_font_style));
            if(tp->m_font_name)
                decls += fmt::format(" font-family: \"{}\";", tp->m_font_name->get_name());
        }
        if(const auto &pp = s->m_paragraph_props)
        {
            if(pp->m_break_before)
                decls += fmt::format(" break-before: {};", css_value(pp->m_break_before->m_break_type));
            if(pp->m_break_after)
                decls += fmt::format(" break-after: {};", css_value(pp->m_break_after->m_break_type));
        }
        if(!decls.empty())
            css += "." + css_class(s->m_name.get_name()) + " {" + decls + " }\n";
    }
    return css;
}

void plain_text(const std::vector<inline_node> &kids, std::string &out)
{
    for(const auto &k : kids)
        std::visit(overloaded{
                       [&](const text &t) { out += t.content; },
                       [&](const span &s) { plain_text(s.children(), out); },
                       [&](const hyperlink &h) { plain_text(h.children(), out); },
                       [](const auto &) {},
                   },
            k.v);
}

/// The page title: the first heading's text, if there is one.
std::string default_title(const text_doc &doc)
{
    for(const auto &b : doc.body)
        if(const auto *h = std::get_if<heading>(&b.v))
        {
            std::string t;
            plain_text(h->children(), t);
            return t;
        }
    return "Untitled";
}

/// The list's numbering at a nesting level, or nullptr for a bulleted (or
/// unstyled) level.
const list_style_num *numbering(const list_style *ls, int level)
{
    if(!ls)
        return nullptr;
    for(const auto &s : ls->m_level_styles)
        if(std::visit([](const auto &x) { return x.m_level; }, s) == level)
            return std::get_if<list_style_num>(&s);
    return nullptr;
}

/// A container holding just one unstyled paragraph is written "tight", e.g.
/// <li>text</li> rather than <li><p>text</p></li>.
const paragraph *sole_plain_paragraph(const std::vector<block_node> &blocks)
{
    if(blocks.size() != 1)
        return nullptr;
    const auto *p = std::get_if<paragraph>(&blocks.front().v);
    return p && p->style.is_empty() ? p : nullptr;
}

/// Where each image goes and what its src is. Document resources go to
/// resource_dir / name. Files on disk already inside resource_dir are referenced
/// in place; other files are copied to resource_dir / Pictures/, renamed if
/// another image already has that name. Other uris (URLs) are left as is.
struct resource_plan
{
    std::map<std::string, std::string> src;          //!< image uri -> src attribute
    std::vector<std::pair<fs::path, fs::path>> copy; //!< Files to copy: from, to

    /// base: the folder of the page being saved, or null when writing to a
    /// string (then src is what save() would write, but nothing is planned
    /// for copying).
    resource_plan(const text_doc &doc, const fs::path &resource_dir, const fs::path *base)
    {
        const fs::path dir = (base ? *base / resource_dir : resource_dir).lexically_normal();
        auto src_of = [&](const fs::path &p)
        { return (base ? p.lexically_relative(*base) : p).generic_string(); };

        std::set<std::string> taken; // names used in dir
        for(const frame *f : collect<frame>(doc))
            if(doc.resources().find(f->img.uri))
                taken.insert(f->img.uri);

        for(const frame *f : collect<frame>(doc))
        {
            const std::string &uri = f->img.uri;
            if(src.contains(uri))
                continue;
            std::error_code ec;
            if(doc.resources().find(uri))
            {
                src[uri] = src_of(dir / uri);
            }
            else if(fs::is_regular_file(uri, ec))
            {
                const fs::path abs = fs::weakly_canonical(uri, ec);
                const fs::path in_dir = abs.lexically_relative(fs::weakly_canonical(dir, ec));
                if(base && !in_dir.empty() && *in_dir.begin() != "..")
                {
                    src[uri] = src_of(abs);
                    continue;
                }
                const fs::path name = fs::path(uri).filename();
                std::string rel = ("Pictures" / name).generic_string();
                for(int i = 2; taken.contains(rel); ++i)
                    rel = (fs::path("Pictures") /
                        (name.stem().string() + "_" + std::to_string(i) +
                            name.extension().string()))
                              .generic_string();
                taken.insert(rel);
                src[uri] = src_of(dir / rel);
                if(base)
                    copy.emplace_back(abs, dir / rel);
            }
            else
            {
                src[uri] = uri;
            }
        }
    }
};

class writer
{
public:
    writer(std::ostream &os, const text_doc &doc, const resource_plan &plan) :
        m_os(os), m_doc(doc), m_plan(plan)
    {
    }

    void page(const std::string &title, const std::string &extra_css)
    {
        m_os << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<title>";
        escape(m_os, title);
        m_os << "</title>\n";

        const std::string css = generated_css(m_doc) + extra_css;
        if(!css.empty())
            m_os << "<style>\n" << css << (css.ends_with('\n') ? "" : "\n") << "</style>\n";

        m_os << "</head>\n<body>\n";
        for(const auto &b : m_doc.body)
            block(b);
        m_os << "</body>\n</html>\n";
    }

private:
    void block(const block_node &b)
    {
        std::visit([this](const auto &n) { emit_block(n); }, b.v);
    }
    void flow(const std::vector<inline_node> &kids)
    {
        for(const auto &k : kids)
            std::visit([this](const auto &n) { emit_inline(n); }, k.v);
    }

    // Block level: each on its own line.
    void emit_block(const paragraph &p)
    {
        indent();
        open("p", p.style);
        flow(p.children());
        m_os << "</p>\n";
    }
    void emit_block(const heading &h)
    {
        const std::string tag = "h" + std::to_string(std::clamp(h.level, 1, 6));
        indent();
        open(tag, h.style);
        flow(h.children());
        m_os << "</" << tag << ">\n";
    }
    void emit_block(const list &l)
    {
        // A nested list usually has no style of its own and continues the
        // enclosing list's style at the next level.
        const list_style *ls = l.style.is_empty()
            ? (m_lists.empty() ? nullptr : m_lists.back())
            : m_doc.list_styles().find(l.style.get_name());
        const auto *num = numbering(ls, static_cast<int>(m_lists.size()) + 1);
        const char *tag = num ? "ol" : "ul";

        indent();
        m_os << '<' << tag;
        class_attr(l.style);
        if(num && num->m_format != list_enum::arabic)
            m_os << " type=\"" << static_cast<char>(num->m_format) << '"';
        if(num && num->m_start_from != 1)
            m_os << " start=\"" << num->m_start_from << '"';
        m_os << ">\n";

        m_lists.push_back(ls);
        ++m_depth;
        for(const auto &it : l.items())
            container("li", style_name{}, it.children());
        --m_depth;
        m_lists.pop_back();

        indent();
        m_os << "</" << tag << ">\n";
    }
    void emit_block(const table &t)
    {
        indent();
        m_os << "<table>\n";
        ++m_depth;
        for(size_t r = 0; r != t.num_rows(); ++r)
        {
            indent();
            m_os << "<tr>\n";
            ++m_depth;
            for(size_t c = 0; c != t.num_cols(); ++c)
            {
                if(t.is_cell_covered(r, c))
                    continue; // spanned over by an earlier cell
                const auto &cell = t.cell(r, c);
                std::string attrs;
                if(cell.rowspan != 1)
                    attrs += fmt::format(" rowspan=\"{}\"", cell.rowspan);
                if(cell.colspan != 1)
                    attrs += fmt::format(" colspan=\"{}\"", cell.colspan);
                container("td", cell.style, cell.children(), attrs);
            }
            --m_depth;
            indent();
            m_os << "</tr>\n";
        }
        --m_depth;
        indent();
        m_os << "</table>\n";
    }
    void emit_block(const frame &f)
    {
        indent();
        img(f);
        m_os << '\n';
    }
    void emit_block(const unknown &) {} // another format's markup; no HTML meaning

    // Inline level: no added whitespace.
    void emit_inline(const text &t) { escape(m_os, t.content); }
    void emit_inline(const span &s)
    {
        if(s.style.is_empty())
            return flow(s.children()); // an unstyled span adds nothing
        open("span", s.style);
        flow(s.children());
        m_os << "</span>";
    }
    void emit_inline(const hyperlink &h)
    {
        m_os << "<a href=\"";
        escape(m_os, h.url.starts_with('#') ? "#" + html_id(h.url.substr(1)) : h.url, true);
        m_os << "\">";
        flow(h.children());
        m_os << "</a>";
    }
    void emit_inline(const frame &f) { img(f); }
    void emit_inline(const bookmark &b)
    {
        m_os << "<a id=\"";
        escape(m_os, html_id(b.name), true);
        m_os << "\"></a>";
    }
    void emit_inline(const unknown &) {}

    /// <li> or <td>: tight when it holds one plain paragraph, else block layout.
    void container(std::string_view tag, const style_name &style,
        const std::vector<block_node> &blocks, const std::string &attrs = {})
    {
        indent();
        m_os << '<' << tag;
        class_attr(style);
        m_os << attrs << '>';
        if(const auto *p = sole_plain_paragraph(blocks))
            flow(p->children());
        else
        {
            m_os << '\n';
            ++m_depth;
            for(const auto &b : blocks)
                block(b);
            --m_depth;
            indent();
        }
        m_os << "</" << tag << ">\n";
    }

    void img(const frame &f)
    {
        m_os << "<img";
        class_attr(f.style);
        m_os << " src=\"";
        escape(m_os, src(f.img.uri), true);
        m_os << "\" alt=\"\">";
    }

    std::string src(const std::string &uri) const
    {
        auto it = m_plan.src.find(uri);
        return it != m_plan.src.end() ? it->second : uri;
    }

    void open(std::string_view tag, const style_name &style)
    {
        m_os << '<' << tag;
        class_attr(style);
        m_os << '>';
    }
    void class_attr(const style_name &style)
    {
        if(style.is_empty())
            return;
        m_os << " class=\"";
        escape(m_os, css_class(style.get_name()), true);
        m_os << '"';
    }
    void indent() { m_os << std::string(m_depth * 2, ' '); }

    std::ostream &m_os;
    const text_doc &m_doc;
    const resource_plan &m_plan;
    int m_depth{0};
    std::vector<const list_style *> m_lists; //!< Enclosing lists' styles, innermost last
};

} // namespace

std::string file::to_string() const
{
    std::ostringstream ss;
    const resource_plan plan(doc, resource_dir, nullptr);
    writer(ss, doc, plan).page(title.empty() ? default_title(doc) : title, css);
    return ss.str();
}

void file::save(const fs::path &p) const
{
    const fs::path base = fs::weakly_canonical(fs::absolute(p).parent_path());
    fs::create_directories(base);

    const resource_plan plan(doc, resource_dir, &base);
    doc.write_resources(base / resource_dir);
    for(const auto &[from, to] : plan.copy)
    {
        fs::create_directories(to.parent_path());
        fs::copy_file(from, to, fs::copy_options::overwrite_existing);
    }

    std::ofstream out(p, std::ios::binary);
    if(!out)
        throw std::runtime_error("Could not write " + p.string());
    writer(out, doc, plan).page(title.empty() ? default_title(doc) : title, css);
}

} // namespace docsmith::html
