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
#include "docsmithcpp/md/file.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "bytes.h"
#include "md/names.h"
#include "resource_plan.h"

// Each block is rendered to a string without a trailing newline, so that a
// container (list item, block quote) can indent or prefix its lines, and blocks
// are joined with a blank line.

namespace docsmith::md
{
namespace fs = std::filesystem;

namespace
{

/// Whether sn is base, or a style defined (through its parents) from base.
bool based_on(const style_name &sn, const style_name &base, const style_registry &styles)
{
    style_name s = sn;
    for(int depth = 0; !s.is_empty() && depth != 32; ++depth) // guards against a cycle
    {
        if(s == base)
            return true;
        const style *def = styles.find(s.get_name());
        if(!def)
            return false;
        s = def->m_parent_style;
    }
    return false;
}

/// What a span's style comes to in Markdown.
struct span_format
{
    bool strong{false};
    bool emphasis{false};
    bool code{false};
};

span_format format_of(const style_name &sn, const style_registry &styles)
{
    span_format f;
    f.code = based_on(sn, builtin::source_text, styles);
    std::optional<font_weight> weight;
    std::optional<font_style> slant;
    const style *def = styles.find(sn.get_name());
    for(int depth = 0; def && depth != 32; ++depth)
    {
        if(const auto &tp = def->m_text_props)
        {
            if(!weight)
                weight = tp->m_font_weight;
            if(!slant)
                slant = tp->m_font_style;
        }
        def = styles.find(def->m_parent_style.get_name());
    }
    // The names alone are enough, as for builtin styles.
    f.strong = weight ? *weight == font_weight::bold : based_on(sn, names::strong, styles);
    f.emphasis = slant ? *slant != font_style::normal : based_on(sn, names::emphasis, styles);
    return f;
}

void plain_text(const std::vector<inline_node> &kids, std::string &out)
{
    for(const auto &k : kids)
        std::visit(
            overloaded{
                [&](const text &t) { out += t.content; },
                [&](const span &s) { plain_text(s.children(), out); },
                [&](const hyperlink &h) { plain_text(h.children(), out); },
                [](const auto &) { },
            },
            k.v);
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

size_t longest_run(std::string_view s, char c)
{
    size_t longest = 0, run = 0;
    for(char x : s)
    {
        run = x == c ? run + 1 : 0;
        longest = std::max(longest, run);
    }
    return longest;
}

bool is_alnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

/// Whether s starts with something that reads as an entity, e.g. "&amp;".
bool starts_entity(std::string_view s)
{
    size_t i = s.size() > 1 && s[1] == '#' ? 2 : 1;
    const size_t start = i;
    while(i != s.size() && is_alnum(s[i]))
        ++i;
    return i != start && i != s.size() && s[i] == ';';
}

std::string attribute(std::string_view s)
{
    std::string out;
    for(char c : s)
    {
        switch(c)
        {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

/// Prefixes every line after the first with indent, leaving blank lines empty.
std::string indent_lines(std::string_view s, std::string_view indent)
{
    std::string out;
    for(size_t i = 0; i != s.size(); ++i)
    {
        out += s[i];
        if(s[i] == '\n' && i + 1 != s.size() && s[i + 1] != '\n')
            out += indent;
    }
    return out;
}

class writer
{
public:
    writer(const text_doc &doc, const detail::resource_plan &plan) :
        m_doc(doc), m_plan(plan)
    {
    }

    std::string document()
    {
        std::string out = blocks(m_doc.body, false);
        return out.empty() ? out : out + '\n';
    }

private:
    /// Where inline content goes, which decides how '\n' and a few characters
    /// are written.
    enum class context
    {
        block,
        heading, // one line
        cell     // one line; '|' splits cells
    };

    const style_registry &styles() const { return m_doc.styles(); }

    // ---- Blocks -------------------------------------------------------------

    std::string blocks(const std::vector<block_node> &bs, bool tight)
    {
        std::string out;
        char last_marker = 0; // of the list just written, if the last block is one
        for(size_t i = 0; i != bs.size(); ++i)
        {
            std::string s;
            if(is_quote(bs[i]))
            {
                size_t j = i;
                while(j + 1 != bs.size() && is_quote(bs[j + 1]))
                    ++j;
                s = quote(bs, i, j + 1);
                i = j;
            }
            else
                s = std::visit([this](const auto &n) { return emit(n); }, bs[i].v);
            if(s.empty())
                continue;
            // A list's first line starts with its marker, '-' or a number.
            const char marker = std::holds_alternative<list>(bs[i].v)
                ? (s.front() == '-' ? '-' : s[s.find_first_not_of("0123456789")])
                : 0;
            if(!out.empty())
                out += tight ? "\n" : "\n\n";
            // Two lists of the same kind in a row would read as one.
            if(marker && marker == last_marker)
                out += "<!-- -->\n\n";
            out += s;
            last_marker = marker;
        }
        return out;
    }

    bool is_quote(const block_node &b) const
    {
        const auto *p = std::get_if<paragraph>(&b.v);
        return p && based_on(p->style, names::quotations, styles()) &&
            !based_on(p->style, builtin::preformatted, styles());
    }

    std::string quote(const std::vector<block_node> &bs, size_t from, size_t to)
    {
        std::string body;
        for(size_t i = from; i != to; ++i)
        {
            const std::string s = inline_block(std::get<paragraph>(bs[i].v).children());
            if(s.empty())
                continue;
            if(!body.empty())
                body += "\n\n";
            body += s;
        }
        if(body.empty())
            return body;
        std::string out = "> ";
        for(size_t i = 0; i != body.size(); ++i)
        {
            out += body[i];
            if(body[i] == '\n')
                out += i + 1 != body.size() && body[i + 1] == '\n' ? ">" : "> ";
        }
        return out;
    }

    std::string emit(const paragraph &p)
    {
        if(based_on(p.style, builtin::preformatted, styles()))
        {
            std::string t;
            plain_text(p.children(), t);
            const std::string fence(std::max<size_t>(3, longest_run(t, '`') + 1), '`');
            return fence + '\n' + t + '\n' + fence;
        }
        if(based_on(p.style, builtin::title, styles()))
            return heading_line(1, p.children());
        return inline_block(p.children());
    }

    std::string emit(const heading &h)
    { return heading_line(std::clamp(h.level, 1, 6), h.children()); }

    std::string emit(const list &l)
    {
        // A nested list usually has no style of its own and continues the
        // enclosing list's style at the next level.
        const list_style *ls = l.style.is_empty() ? (m_lists.empty() ? nullptr : m_lists.back())
                                                  : m_doc.list_styles().find(l.style.get_name());
        const auto *num = numbering(ls, static_cast<int>(m_lists.size()) + 1);
        const bool tight = std::all_of(l.items().begin(),
            l.items().end(),
            [&](const list_item &it) { return is_tight(it, ls); });

        m_lists.push_back(ls);
        std::string out;
        int n = num ? num->m_start_from : 0;
        for(const auto &it : l.items())
        {
            const std::string marker = num
                ? std::to_string(n++) + (num->m_num_suffix == ")" ? ")" : ".")
                : std::string("-");
            const std::string body = blocks(it.children(), tight);
            if(!out.empty())
                out += tight ? "\n" : "\n\n";
            out += marker;
            if(!body.empty())
                out += ' ' + indent_lines(body, std::string(marker.size() + 1, ' '));
        }
        m_lists.pop_back();
        return out;
    }

    /// Whether an item can be written without blank lines: a plain paragraph,
    /// optionally followed by a list that can interrupt it.
    bool is_tight(const list_item &it, const list_style *ls) const
    {
        const auto &bs = it.children();
        auto plain = [this](const block_node &b)
        {
            const auto *p = std::get_if<paragraph>(&b.v);
            return p && !based_on(p->style, builtin::preformatted, styles()) &&
                !based_on(p->style, names::quotations, styles());
        };
        auto starts_at_1 = [&](const block_node &b)
        {
            const auto &l = std::get<list>(b.v);
            const list_style *nested =
                l.style.is_empty() ? ls : m_doc.list_styles().find(l.style.get_name());
            const auto *num = numbering(nested, static_cast<int>(m_lists.size()) + 2);
            return !num || num->m_start_from == 1;
        };
        if(bs.size() > 2)
            return false;
        if(bs.size() == 1)
            return plain(bs[0]) || std::holds_alternative<list>(bs[0].v);
        if(bs.size() == 2)
            return plain(bs[0]) && std::holds_alternative<list>(bs[1].v) && starts_at_1(bs[1]);
        return true;
    }

    std::string emit(const table &t)
    {
        const size_t cols = t.num_cols();
        if(cols == 0)
            return {};
        auto row = [&](size_t r)
        {
            std::string line = "|";
            for(size_t c = 0; c != cols; ++c)
            {
                const std::string s = t.is_cell_covered(r, c) ? std::string() : cell(t.cell(r, c));
                line += s.empty() ? " |" : ' ' + s + " |";
            }
            return line;
        };

        // GFM tables always have a header row. Without one, it is left empty.
        std::string out = "|";
        if(t.header_rows != 0)
            out = row(0);
        else
            for(size_t c = 0; c != cols; ++c)
                out += " |";
        out += "\n|";
        for(size_t c = 0; c != cols; ++c)
            out += " --- |";
        for(size_t r = t.header_rows != 0 ? 1 : 0; r != t.num_rows(); ++r)
            out += '\n' + row(r);
        return out;
    }

    /// A cell on one line: its paragraphs separated by <br>, and nested lists
    /// and tables as their text.
    std::string cell(const table_cell &c)
    {
        std::string out;
        auto part = [&](const std::string &s)
        {
            if(s.empty())
                return;
            if(!out.empty())
                out += "<br>";
            out += s;
        };
        for(const auto &b : c.children())
            cell_block(b, part);
        return out;
    }

    template <typename Part>
    void cell_block(const block_node &b, Part &part)
    {
        std::visit(
            overloaded{
                [&](const paragraph &p) { part(inlines(p.children(), context::cell)); },
                [&](const heading &h) { part(inlines(h.children(), context::cell)); },
                [&](const frame &f) { part(image(f)); },
                [&](const list &l)
                {
                    for(const auto &it : l.items())
                        for(const auto &ib : it.children())
                            cell_block(ib, part);
                },
                [&](const table &t)
                {
                    for(const auto &r : t.rows)
                        for(const auto &tc : r)
                            for(const auto &cb : tc.children())
                                cell_block(cb, part);
                },
                [](const auto &) { },
            },
            b.v);
    }

    std::string emit(const frame &f) { return image(f); }
    std::string emit(const page_break &) { return "---"; }
    std::string emit(const unknown &) { return {}; } // another format's markup

    std::string heading_line(int level, const std::vector<inline_node> &kids)
    {
        std::string s = inlines(kids, context::heading);
        while(!s.empty() && s.back() == ' ')
            s.pop_back();
        return std::string(level, '#') + (s.empty() ? "" : " " + s);
    }

    /// A paragraph's content, starting a line. A hard break at the end would be
    /// a literal backslash, so it is dropped.
    std::string inline_block(const std::vector<inline_node> &kids)
    {
        m_bol = true;
        std::string s = inlines(kids, context::block);
        while(s.ends_with("\\\n"))
            s.resize(s.size() - 2);
        return s;
    }

    // ---- Inline content -----------------------------------------------------

    std::string inlines(const std::vector<inline_node> &kids, context ctx)
    {
        if(ctx != context::block)
            m_bol = false;
        std::string out;
        for(const auto &k : kids)
            std::visit([&](const auto &n) { emit(n, ctx, out); }, k.v);
        return out;
    }

    void emit(const text &t, context ctx, std::string &out) { put_text(t.content, ctx, out); }

    void emit(const span &s, context ctx, std::string &out)
    {
        const span_format f = s.style.is_empty() ? span_format{} : format_of(s.style, styles());
        if(f.code)
        {
            std::string t;
            plain_text(s.children(), t);
            out += code_span(t, ctx);
            m_bol = false;
            return;
        }
        if(!f.strong && !f.emphasis)
        {
            for(const auto &k : s.children())
                std::visit([&](const auto &n) { emit(n, ctx, out); }, k.v);
            return;
        }

        const bool bol = m_bol;
        m_bol = false;
        std::string inner;
        for(const auto &k : s.children())
            std::visit([&](const auto &n) { emit(n, ctx, inner); }, k.v);

        // A delimiter run next to white space doesn't open or close emphasis,
        // so leading and trailing spaces and breaks go outside it.
        size_t lead = 0;
        while(lead != inner.size() && inner[lead] == ' ')
            ++lead;
        size_t end = inner.size();
        for(;;)
        {
            if(end > lead && inner[end - 1] == ' ')
                --end;
            else if(end >= lead + 2 && inner.compare(end - 2, 2, "\\\n") == 0)
                end -= 2;
            else
                break;
        }
        if(lead == end)
        {
            m_bol = bol && inner.empty();
            out += inner;
            return;
        }
        const std::string delim = std::string(f.strong ? "**" : "") + (f.emphasis ? "*" : "");
        out += inner.substr(0, lead) + delim + inner.substr(lead, end - lead) + delim +
            inner.substr(end);
        m_bol = inner.ends_with('\n');
    }

    void emit(const hyperlink &h, context ctx, std::string &out)
    {
        const auto *u = std::get_if<url_target>(&h.target);
        if(u && h.children().size() == 1)
            if(const auto *t = std::get_if<text>(&h.children().front().v); t &&
                t->content == u->url && u->url.find("://") != std::string::npos &&
                u->url.find_first_of(" <>\n") == std::string::npos)
            {
                out += '<' + (ctx == context::cell ? escape_pipes(u->url) : u->url) + '>';
                m_bol = false;
                return;
            }

        out += '[';
        m_bol = false;
        for(const auto &k : h.children())
            std::visit([&](const auto &n) { emit(n, ctx, out); }, k.v);
        const std::string dest =
            u ? u->url : "#" + names::anchor_id(std::get<bookmark_target>(h.target).name);
        out += "](" + destination(dest, ctx) + ')';
        m_bol = false;
    }

    void emit(const frame &f, context, std::string &out)
    {
        out += image(f);
        m_bol = false;
    }

    void emit(const bookmark &b, context, std::string &out)
    {
        out += "<a id=\"" + attribute(names::anchor_id(b.name)) + "\"></a>";
        m_bol = false;
    }

    void emit(const unknown &, context, std::string &) { }

    std::string image(const frame &f)
    { return "![](" + destination(m_plan.src_of(f.img.uri), context::block) + ')'; }

    static std::string escape_pipes(std::string_view s)
    {
        std::string out;
        for(char c : s)
        {
            if(c == '|')
                out += '\\';
            out += c;
        }
        return out;
    }

    /// A link destination, in <> when it has spaces or brackets.
    static std::string destination(std::string_view url, context ctx)
    {
        int depth = 0;
        bool angle = url.empty();
        for(char c : url)
        {
            if(c == '(')
                ++depth;
            else if(c == ')' && --depth < 0)
                angle = true;
            else if(c == ' ' || c == '<' || c == '>' || static_cast<unsigned char>(c) < 0x20)
                angle = true;
        }
        angle = angle || depth != 0;
        std::string out;
        for(char c : url)
        {
            if(c == '\\' || (angle && (c == '<' || c == '>')) || (c == '|' && ctx == context::cell))
                out += '\\';
            if(c != '\n')
                out += c;
        }
        return angle ? '<' + out + '>' : out;
    }

    static std::string code_span(std::string s, context ctx)
    {
        std::replace(s.begin(), s.end(), '\n', ' ');
        if(s.empty())
            return s;
        if(ctx == context::cell)
            s = escape_pipes(s);
        const std::string ticks(longest_run(s, '`') + 1, '`');
        // One space each side is stripped when both are there, so it has to be
        // added back, and a backtick at either end needs a space to stay apart.
        const bool pad = s.front() == '`' || s.back() == '`' ||
            (s.front() == ' ' && s.back() == ' ' && s.find_first_not_of(' ') != std::string::npos);
        return pad ? ticks + ' ' + s + ' ' + ticks : ticks + s + ticks;
    }

    /// Text with the characters Markdown would read as markup escaped.
    void put_text(std::string_view s, context ctx, std::string &out)
    {
        for(size_t i = 0; i != s.size(); ++i)
        {
            const char c = s[i];
            if(c == '\r')
                continue;
            if(c == '\n')
            {
                if(ctx == context::block)
                {
                    out += "\\\n";
                    m_bol = true;
                }
                else
                    out += ctx == context::cell ? "<br>" : " ";
                continue;
            }
            if(m_bol)
            {
                m_bol = false;
                // Leading white space would be dropped, or start a code block.
                if(c == ' ')
                {
                    out += "&#32;";
                    continue;
                }
                if(c == '\t')
                {
                    out += "&#9;";
                    continue;
                }
                if(c == '#' || c == '>' || c == '-' || c == '+' || c == '=')
                {
                    out += '\\';
                    out += c;
                    continue;
                }
                if(std::isdigit(static_cast<unsigned char>(c)))
                {
                    size_t j = i;
                    while(j != s.size() && std::isdigit(static_cast<unsigned char>(s[j])))
                        ++j;
                    if(j != s.size() && (s[j] == '.' || s[j] == ')'))
                    {
                        out.append(s.substr(i, j - i));
                        out += '\\';
                        out += s[j];
                        i = j;
                        continue;
                    }
                }
            }
            bool esc = false;
            switch(c)
            {
            case '\\':
            case '`':
            case '*':
            case '[':
            case ']':
            case '<':
            case '|':
            case '~': esc = true; break;
            case '_': // only at a word boundary, so snake_case stays readable
                esc = i == 0 || i + 1 == s.size() || !is_alnum(s[i - 1]) || !is_alnum(s[i + 1]);
                break;
            case '&': esc = starts_entity(s.substr(i)); break;
            case '#': esc = ctx == context::heading; break; // a closing sequence
            default: break;
            }
            if(esc)
                out += '\\';
            out += c;
        }
    }

    const text_doc &m_doc;
    const detail::resource_plan &m_plan;
    std::vector<const list_style *> m_lists; //!< Enclosing lists' styles, innermost last
    bool m_bol{false};                       //!< At the start of a line
};

} // namespace

std::string file::to_string() const
{
    const detail::resource_plan plan(doc, resource_dir, nullptr);
    return writer(doc, plan).document();
}

void file::save(const fs::path &p) const
{
    const fs::path base = fs::weakly_canonical(fs::absolute(p).parent_path());
    fs::create_directories(base);

    const detail::resource_plan plan(doc, resource_dir, &base);
    plan.write(doc, base, resource_dir);
    detail::write_bytes(p, writer(doc, plan).document());
}

} // namespace docsmith::md
