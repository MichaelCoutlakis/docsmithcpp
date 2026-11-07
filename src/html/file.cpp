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
#include "docsmithcpp/error.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

#include "cell_text.h"
#include "format.h"
#include "lineage.h"
#include "list_numbers.h"
#include "outline.h"

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

const char *css_value(font_weight fw) { return fw == font_weight::bold ? "bold" : "normal"; }

const char *css_value(text_align a)
{
    switch(a)
    {
    case text_align::start: return "start";
    case text_align::end: return "end";
    case text_align::left: return "left";
    case text_align::right: return "right";
    case text_align::centre: return "center";
    case text_align::justify: return "justify";
    }
    return "start";
}

const char *css_value(border_line l)
{
    switch(l)
    {
    case border_line::none: return "none";
    case border_line::dashed: return "dashed";
    case border_line::dotted: return "dotted";
    case border_line::double_line: return "double";
    default: return "solid";
    }
}

std::string css_value(const border &b)
{
    if(b.m_line == border_line::none)
        return "none";
    return detail::to_string(b.m_width.m_points) + "pt " + css_value(b.m_line) + " " +
        detail::to_hex(b.m_colour);
}

const char *css_value(cell_valign v)
{
    switch(v)
    {
    case cell_valign::middle: return "middle";
    case cell_valign::bottom: return "bottom";
    default: return "top";
    }
}

/// Declarations for a cell, each starting with a space.
std::string css_decls(const cell_props &p)
{
    std::string out;
    auto side = [&](const char *name, const std::optional<border> &b)
    {
        if(b)
            out += std::string(" border-") + name + ": " + css_value(*b) + ";";
    };
    side("top", p.m_borders.m_top);
    side("bottom", p.m_borders.m_bottom);
    side("left", p.m_borders.m_start);
    side("right", p.m_borders.m_end);
    if(p.m_background)
        out += " background-color: " + detail::to_hex(*p.m_background) + ";";
    if(p.m_valign)
        out += std::string(" vertical-align: ") + css_value(*p.m_valign) + ";";
    if(p.m_padding)
        out += " padding: " + detail::to_string(p.m_padding->m_points) + "pt;";
    return out;
}

/// Declarations for a table style: its width and alignment. The lines and
/// header go on the cells (see emit_block(table)).
std::string css_decls(const table_props &p)
{
    std::string out = " border-collapse: collapse;";
    if(p.m_width)
        out += " width: " +
            std::visit(
                overloaded{
                    [](const length &l) { return detail::to_string(l.m_points) + "pt"; },
                    [](const percent &pc) { return detail::to_string(pc.m_value) + "%"; },
                },
                *p.m_width) +
            ";";
    if(p.m_align == table_align::centre)
        out += " margin-left: auto; margin-right: auto;";
    else if(p.m_align == table_align::end)
        out += " margin-left: auto;";
    return out;
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

template <typename T>
void fill(std::optional<T> &into, const std::optional<T> &from)
{
    if(!into)
        into = from;
}

/// s with the properties it inherits from its parent styles filled in, since
/// a CSS class doesn't inherit from another.
template <typename S>
S resolved(const S &s, const style_sheet &styles)
{
    S out = s;
    for(const S *parent : detail::lineage(s.parent, styles)) // nearest first
    {
        text_props tp = parent->text;
        out.text = tp.overlay(out.text);
        if constexpr(requires { out.para; })
        {
            paragraph_props pp = parent->para;
            out.para = pp.overlay(out.para);
        }
    }
    return out;
}

/// Whether sn is base, or a style defined (through its parents) from base.
template <style_family F>
bool based_on(const style_name<F> &sn, const style_name<F> &base, const style_sheet &styles)
{
    style_name<F> s = sn;
    for(int depth = 0; !s.empty() && depth != 32; ++depth) // guards against a cycle
    {
        if(s == base)
            return true;
        const auto *def = styles.find(s);
        if(!def)
            return false;
        s = def->parent;
    }
    return false;
}

std::string css_decls(const text_props &tp)
{
    std::string decls;
    if(tp.m_font_size)
        decls += " font-size: " + detail::to_string(tp.m_font_size->m_points) + "pt;";
    if(tp.m_font_style)
        decls += std::string(" font-style: ") + css_value(*tp.m_font_style) + ";";
    if(tp.m_font_name)
        decls += " font-family: \"" + tp.m_font_name->str() + "\";";
    if(tp.m_font_weight)
        decls += std::string(" font-weight: ") + css_value(*tp.m_font_weight) + ";";
    if(const auto &c = tp.m_font_colour)
        decls += " color: " + detail::to_hex(*c) + ";";
    if(tp.m_underline)
        decls += *tp.m_underline == underline::single ? " text-decoration: underline;"
                                                      : " text-decoration: none;";
    return decls;
}

std::string css_decls(const paragraph_props &pp)
{
    std::string decls;
    if(pp.m_break_before)
        decls += std::string(" break-before: ") + css_value(pp.m_break_before->m_break_type) + ";";
    if(pp.m_break_after)
        decls += std::string(" break-after: ") + css_value(pp.m_break_after->m_break_type) + ";";
    auto len = [&](const char *prop, const std::optional<length> &l)
    {
        if(l)
            decls += std::string(" ") + prop + ": " + detail::to_string(l->m_points) + "pt;";
    };
    len("margin-top", pp.m_space_before);
    len("margin-bottom", pp.m_space_after);
    len("margin-left", pp.m_indent_start);
    len("margin-right", pp.m_indent_end);
    len("text-indent", pp.m_first_line_indent);
    if(pp.m_align)
        decls += std::string(" text-align: ") + css_value(*pp.m_align) + ";";
    if(pp.m_keep_with_next.value_or(false) && !pp.m_break_after)
        decls += " break-after: avoid;";
    if(pp.m_keep_together.value_or(false))
        decls += " break-inside: avoid;";
    return decls;
}

/// The elements a family's styles are written on.
const char *elements_of(style_family f)
{
    switch(f)
    {
    case style_family::para: return "p, pre, h1, h2, h3, h4, h5, h6";
    case style_family::text: return "span, code, a";
    case style_family::table: return "table";
    case style_family::cell: return "td, th";
    default: return "*";
    }
}

/// The selector for a style's rule: its class, and for a default style the
/// elements with no class it applies to. qualified: the name is used in
/// another family too, so the class is limited to this family's elements.
template <style_family F>
std::string selector(const style_name<F> &sn, bool qualified)
{
    std::string out = "." + css_class(sn.str());
    if(qualified)
        out = std::string(":is(") + elements_of(F) + ")" + out;
    auto also = [&](const std::string &element) { out += ", " + element + ":not([class])"; };
    if constexpr(F == style_family::para)
    {
        if(sn == builtin::para)
            also("p");
        else
            for(int level = 1; level <= 6; ++level)
                if(sn == builtin::heading(level))
                    also("h" + std::to_string(level));
    }
    else if constexpr(F == style_family::text)
    {
        if(sn == builtin::link)
            also("a");
    }
    else if constexpr(F == style_family::table)
    {
        if(sn == builtin::table)
            also("table");
    }
    return out;
}

/// CSS rules for the style definitions in the document, by family and then
/// name, so the output is stable.
std::string generated_css(const text_doc &doc)
{
    const style_sheet &styles = doc.styles();
    // Names defined in more than one of the families written as classes.
    std::map<std::string, int> uses;
    for(const auto &[name, s] : styles.para)
        ++uses[name.str()];
    for(const auto &[name, s] : styles.text)
        ++uses[name.str()];
    for(const auto &[name, s] : styles.table)
        ++uses[name.str()];
    for(const auto &[name, s] : styles.cell)
        ++uses[name.str()];
    auto rule = [&](const auto &name, const std::string &decls)
    {
        return decls.empty() ? std::string()
                             : selector(name, uses[name.str()] > 1) + " {" + decls + " }\n";
    };

    std::string css;
    for(const auto &[name, s] : styles.para)
    {
        const paragraph_style r = resolved(s, styles);
        css += rule(name, css_decls(r.text) + css_decls(r.para));
    }
    for(const auto &[name, s] : styles.text)
        css += rule(name, css_decls(resolved(s, styles).text));
    for(const auto &[name, s] : styles.table)
        css += rule(name, css_decls(effective_table_style(name, styles)));
    for(const auto &[name, s] : styles.cell)
        css += rule(name, css_decls(effective_cell_style(name, styles)));
    return css;
}

using detail::plain_text;

/// The page title: the document's title, else the title paragraph's or the
/// first heading's text.
std::string default_title(const text_doc &doc)
{
    if(!doc.meta.title.empty())
        return doc.meta.title;
    if(const paragraph *p = doc.find_title())
    {
        std::string t;
        plain_text(p->items(), t);
        return t;
    }
    for(const auto &b : doc.body)
        if(const auto *h = std::get_if<heading>(&b.v))
        {
            std::string t;
            plain_text(h->items(), t);
            return t;
        }
    return "Untitled";
}

/// A container holding just one unstyled paragraph is written "tight", e.g.
/// <li>text</li> rather than <li><p>text</p></li>.
const paragraph *sole_plain_paragraph(const std::vector<block_node> &blocks)
{
    if(blocks.size() != 1)
        return nullptr;
    const auto *p = std::get_if<paragraph>(&blocks.front().v);
    return p && p->style.empty() ? p : nullptr;
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
        m_os(os), m_doc(doc), m_plan(plan), m_ids(doc, html_id)
    {
    }

    void page(const std::string &title, const std::string &extra_css)
    {
        const metadata &meta = m_doc.meta;
        m_os << "<!DOCTYPE html>\n<html";
        if(!meta.language.empty())
        {
            m_os << " lang=\"";
            escape(m_os, meta.language, true);
            m_os << '"';
        }
        m_os << ">\n<head>\n<meta charset=\"utf-8\">\n<title>";
        escape(m_os, title);
        m_os << "</title>\n";

        std::string keywords;
        for(const auto &k : meta.keywords)
            keywords += (keywords.empty() ? "" : ", ") + k;
        meta_tag("author", meta.author);
        meta_tag("description", meta.description);
        meta_tag("keywords", keywords);

        const std::string css = generated_css(m_doc) + extra_css;
        if(!css.empty())
            m_os << "<style>\n" << css << (css.ends_with('\n') ? "" : "\n") << "</style>\n";

        m_os << "</head>\n<body>\n";
        section("header", m_doc.page.header);
        for(const auto &b : m_doc.body)
            block(b);
        section("footer", m_doc.page.footer);
        m_os << "</body>\n</html>\n";
    }

private:
    void meta_tag(std::string_view name, std::string_view content)
    {
        if(content.empty())
            return;
        m_os << "<meta name=\"" << name << "\" content=\"";
        escape(m_os, content, true);
        m_os << "\">\n";
    }

    /// <header> or <footer>, if there is one.
    void section(std::string_view tag, const std::vector<block_node> &blocks)
    {
        if(blocks.empty())
            return;
        m_os << '<' << tag << ">\n";
        ++m_depth;
        for(const auto &b : blocks)
            block(b);
        --m_depth;
        m_os << "</" << tag << ">\n";
    }

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
        if(based_on(p.style, builtin::preformatted, m_doc.styles()))
        {
            open("pre", p.style);
            std::string t;
            plain_text(p.items(), t);
            if(t.starts_with('\n'))
                m_os << '\n'; // a newline straight after <pre> is dropped
            m_pre = true;
            flow(p.items());
            m_pre = false;
            m_os << "</pre>\n";
            return;
        }
        open("p", p.style);
        flow(p.items());
        m_os << "</p>\n";
    }
    void emit_block(const heading &h)
    {
        const std::string tag = "h" + std::to_string(std::clamp(h.level, 1, 6));
        indent();
        m_os << '<' << tag;
        if(m_ids.own.contains(&h))
            m_os << " id=\"" << m_ids.link.at(&h) << '"';
        class_attr(h.style);
        m_os << '>';
        flow(h.items());
        m_os << "</" << tag << ">\n";
    }
    void emit_block(const list &l)
    {
        // A nested list usually has no style of its own and continues the
        // enclosing list's style at the next level.
        const list_style *ls = l.style.empty()
            ? (m_lists.empty() ? m_doc.styles().find(builtin::list) : m_lists.back())
            : m_doc.styles().find(l.style);
        const int level = static_cast<int>(m_lists.size()) + 1;
        const auto *num = detail::numbering(ls, level);
        const char *tag = num ? "ol" : "ul";
        const auto numbers = num ? m_counter.numbers(l, ls, *num, level)
                                 : std::vector<std::optional<int>>(l.items().size());
        // <ol> counts every <li>, an unnumbered one too, so an item whose
        // number isn't the next one says so.
        int next = !numbers.empty() && numbers.front() ? *numbers.front() : 1;

        indent();
        m_os << '<' << tag;
        class_attr(l.style);
        if(num && num->format != list_enum::arabic)
            m_os << " type=\"" << static_cast<char>(num->format) << '"';
        if(next != 1)
            m_os << " start=\"" << next << '"';
        m_os << ">\n";

        m_lists.push_back(ls);
        ++m_depth;
        for(size_t i = 0; i != l.items().size(); ++i)
        {
            const list_item &it = l.items()[i];
            std::string attrs;
            if(!it.numbered)
                attrs = " style=\"list-style-type: none\"";
            else if(numbers[i] && *numbers[i] != next)
                attrs = " value=\"" + std::to_string(*numbers[i]) + '"';
            next = numbers[i].value_or(next) + 1;
            container("li", {}, it.items(), attrs);
        }
        --m_depth;
        m_lists.pop_back();

        indent();
        m_os << "</" << tag << ">\n";
    }
    void emit_block(const table &t)
    {
        // <thead> / <tbody> only when there are header rows, so a plain table
        // stays plain.
        const size_t header_rows = std::min(t.header_rows(), t.num_rows());
        auto section = [&](size_t r)
        {
            if(header_rows == 0 || (r != 0 && r != header_rows))
                return;
            if(r != 0)
            {
                --m_depth;
                indent();
                m_os << "</thead>\n";
            }
            indent();
            m_os << (r == 0 ? "<thead>\n" : "<tbody>\n");
            ++m_depth;
        };

        // Each cell's formatting, with the table style's lines, in its style
        // attribute: CSS selectors for a table's edges break with spans.
        std::vector<std::string> cell_css(t.num_rows() * t.num_cols());
        std::vector<para_style_name> cell_paragraph(t.num_rows() * t.num_cols());
        bool lines = false;
        for(size_t r = 0; r != t.num_rows(); ++r)
            for(size_t c = 0; c != t.num_cols(); ++c)
                if(!t.is_cell_covered(r, c))
                {
                    const auto p = effective_cell_props(t, m_doc.styles(), r, c);
                    lines = lines || p.m_borders != cell_borders{};
                    cell_css[r * t.num_cols() + c] = css_decls(p);
                    cell_paragraph[r * t.num_cols() + c] = p.m_paragraph_style;
                }

        indent();
        m_os << "<table";
        class_attr(t.style);
        if(const auto &label =
                t.accessibility_title.empty() ? t.accessibility_description : t.accessibility_title;
            !label.empty())
        {
            m_os << " aria-label=\"";
            escape(m_os, label, true);
            m_os << '"';
        }
        if(lines && t.style.empty())
            m_os << " style=\"border-collapse: collapse;\"";
        m_os << ">\n";
        ++m_depth;
        bool widths = false;
        for(size_t c = 0; c != t.num_cols(); ++c)
            widths = widths || t.column_width(c);
        if(widths)
        {
            indent();
            m_os << "<colgroup>";
            for(double share : column_shares(t))
                m_os << "<col style=\"width: " << detail::to_string(share * 100) << "%;\">";
            m_os << "</colgroup>\n";
        }
        for(size_t r = 0; r != t.num_rows(); ++r)
        {
            section(r);
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
                    attrs += " rowspan=\"" + std::to_string(cell.rowspan) + "\"";
                if(cell.colspan != 1)
                    attrs += " colspan=\"" + std::to_string(cell.colspan) + "\"";
                if(const auto &css = cell_css[r * t.num_cols() + c]; !css.empty())
                    attrs += " style=\"" + css.substr(1) + "\"";
                container(r < header_rows ? "th" : "td",
                    cell.style.str(),
                    cell.items(),
                    attrs,
                    cell_paragraph[r * t.num_cols() + c].str());
            }
            --m_depth;
            indent();
            m_os << "</tr>\n";
        }
        if(header_rows != 0)
        {
            --m_depth;
            indent();
            m_os << (header_rows == t.num_rows() ? "</thead>\n" : "</tbody>\n");
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
    void emit_block(const page_break &)
    {
        indent();
        m_os << "<div style=\"break-after: page\"></div>\n";
    }
    /// Nested lists of links to the headings: a heading one level below the
    /// one before it starts a list inside that one's item.
    void emit_block(const table_of_contents &toc)
    {
        indent();
        m_os << "<nav class=\"toc\">\n";
        ++m_depth;
        if(!toc.title.empty())
        {
            indent();
            m_os << "<p class=\"toc-title\">";
            escape(m_os, toc.title);
            m_os << "</p>\n";
        }
        std::vector<bool> item_open; // per open <ul>
        auto close_item = [&]
        {
            if(!item_open.back())
                return;
            --m_depth;
            indent();
            m_os << "</li>\n";
            item_open.back() = false;
        };
        auto close_list = [&]
        {
            close_item();
            item_open.pop_back();
            --m_depth;
            indent();
            m_os << "</ul>\n";
        };
        for(const heading *h : detail::body_headings(m_doc))
        {
            if(h->level > toc.levels)
                continue;
            // A jump of more than one level nests only one list deeper.
            const size_t depth = std::clamp<size_t>(h->level, 1, item_open.size() + 1);
            while(item_open.size() > depth)
                close_list();
            if(item_open.size() == depth)
                close_item();
            while(item_open.size() < depth)
            {
                indent();
                m_os << "<ul>\n";
                ++m_depth;
                item_open.push_back(false);
            }
            indent();
            m_os << "<li><a href=\"#";
            escape(m_os, m_ids.link.at(h), true);
            m_os << "\">";
            escape(m_os, plain_text(h->items()));
            m_os << "</a>\n";
            ++m_depth;
            item_open.back() = true;
        }
        while(!item_open.empty())
            close_list();
        --m_depth;
        indent();
        m_os << "</nav>\n";
    }
    void emit_block(const unknown &) {} // another format's markup; no HTML meaning

    // Inline level: no added whitespace.
    // HTML collapses white space, so outside <pre> a run of spaces keeps all but
    // one as &nbsp;, a tab is kept by white-space: pre, and '\n' is <br>.
    void emit_inline(const text &t)
    {
        if(m_pre)
            return escape(m_os, t.content);
        const std::string &s = t.content;
        for(size_t i = 0; i != s.size(); ++i)
        {
            const char c = s[i];
            if(c == '\n')
                m_os << "<br>";
            else if(c == '\t')
                m_os << "<span style=\"white-space: pre\">\t</span>";
            else if(c == ' ' && i + 1 != s.size() && s[i + 1] == ' ')
                m_os << "&nbsp;";
            else if(c != '\r')
                escape(m_os, std::string_view(&s[i], 1));
        }
    }
    void emit_inline(const span &s)
    {
        if(s.style.empty())
            return flow(s.items()); // an unstyled span adds nothing
        const char *tag = based_on(s.style, builtin::source_text, m_doc.styles()) ? "code" : "span";
        open(tag, s.style);
        flow(s.items());
        m_os << "</" << tag << '>';
    }
    void emit_inline(const hyperlink &h)
    {
        m_os << "<a";
        class_attr(h.style);
        m_os << " href=\"";
        escape(m_os,
            std::visit(overloaded{[](const url_target &u) { return u.url; },
                           [](const bookmark_target &b) { return "#" + html_id(b.name); }},
                h.target),
            true);
        m_os << "\">";
        flow(h.items());
        m_os << "</a>";
    }
    void emit_inline(const frame &f) { img(f); }
    void emit_inline(const bookmark &b)
    {
        m_os << "<a id=\"";
        escape(m_os, html_id(b.name), true);
        m_os << "\"></a>";
    }
    // HTML has no pages, so a page number or count is left out.
    void emit_inline(const field &f)
    {
        if(f.type == field_type::title)
            escape(m_os, m_doc.meta.title);
        else if(f.type == field_type::date)
            m_os << detail::today();
    }
    void emit_inline(const unknown &) {}

    /// <li> or <td>: tight when it holds one plain paragraph, else block layout.
    /// extra: a second class, e.g. the paragraph style of a header row's text.
    void container(std::string_view tag, const std::string &style,
        const std::vector<block_node> &blocks, const std::string &attrs = {},
        const std::string &extra = {})
    {
        indent();
        m_os << '<' << tag;
        class_attr(style, extra);
        m_os << attrs << '>';
        if(const auto *p = sole_plain_paragraph(blocks))
            flow(p->items());
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
        m_os << '"';
        if(f.width || f.height)
        {
            m_os << " style=\"";
            if(f.width)
                m_os << "width: " << detail::to_string(f.width->m_points) << "pt;";
            if(f.height)
                m_os << (f.width ? " " : "") << "height: " << detail::to_string(f.height->m_points)
                     << "pt;";
            m_os << '"';
        }
        // The title is the short alternative alt wants; else the description.
        m_os << " alt=\"";
        escape(m_os,
            f.accessibility_title.empty() ? f.accessibility_description : f.accessibility_title,
            true);
        m_os << "\">";
    }

    std::string src(const std::string &uri) const
    {
        auto it = m_plan.src.find(uri);
        return it != m_plan.src.end() ? it->second : uri;
    }

    template <style_family F>
    void open(std::string_view tag, const style_name<F> &style)
    {
        m_os << '<' << tag;
        class_attr(style);
        m_os << '>';
    }
    template <style_family F>
    void class_attr(const style_name<F> &style)
    { class_attr(style.str()); }
    void class_attr(const std::string &style, const std::string &extra = {})
    {
        if(style.empty() && extra.empty())
            return;
        m_os << " class=\"";
        if(!style.empty())
            escape(m_os, css_class(style), true);
        if(!style.empty() && !extra.empty())
            m_os << ' ';
        if(!extra.empty())
            escape(m_os, css_class(extra), true);
        m_os << '"';
    }
    void indent() { m_os << std::string(m_depth * 2, ' '); }

    std::ostream &m_os;
    const text_doc &m_doc;
    const resource_plan &m_plan;
    int m_depth{0};
    bool m_pre{false};                       //!< Inside <pre>: text is written as is
    std::vector<const list_style *> m_lists; //!< Enclosing lists' styles, innermost last
    detail::list_counter m_counter;
    const detail::heading_ids m_ids;
};

} // namespace

std::string file::to_string() const
{
    const auto with_cell_text = detail::with_cell_text(doc);
    const text_doc &d = with_cell_text ? *with_cell_text : doc;
    std::ostringstream ss;
    const resource_plan plan(d, resource_dir, nullptr);
    writer(ss, d, plan).page(title.empty() ? default_title(d) : title, css);
    return ss.str();
}

namespace
{
void write_page(const text_doc &doc, const std::string &title, const std::string &css,
    const fs::path &resource_dir, const fs::path &p)
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
        throw error(error_code::io, "Could not write " + p.string());
    const auto with_cell_text = detail::with_cell_text(doc);
    writer(out, with_cell_text ? *with_cell_text : doc, plan)
        .page(title.empty() ? default_title(doc) : title, css);
}
} // namespace

void file::save(const fs::path &p) const { write_page(doc, title, css, resource_dir, p); }

void save(const text_doc &doc, const fs::path &p) { write_page(doc, {}, {}, {}, p); }

} // namespace docsmith::html
