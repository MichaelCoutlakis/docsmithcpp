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
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <md4c.h>

#include "bytes.h"
#include "docsmithcpp/md/file.h"
#include "md/names.h"

// md4c reports the document as nested enter / leave calls, so the tree is built
// with a stack: one of the containers blocks go into, and one of the containers
// inline content goes into. A node's children are only appended to while it is
// the innermost open node, so the pointers into its parent stay valid.

namespace docsmith::md
{
namespace fs = std::filesystem;

namespace
{

void append_utf8(std::string &out, std::uint32_t cp)
{
    if(cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        cp = 0xFFFD;
    if(cp < 0x80)
        out += static_cast<char>(cp);
    else if(cp < 0x800)
    {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if(cp < 0x10000)
    {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else
    {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

/// An entity such as "&amp;" or "&#32;" as UTF-8. md4c passes entities on as
/// written. Named entities beyond the common ones are kept as written.
std::string decode_entity(std::string_view e)
{
    std::string out;
    if(e.size() > 3 && e[1] == '#')
    {
        const bool hex = e[2] == 'x' || e[2] == 'X';
        std::uint32_t cp = 0;
        for(char c : e.substr(hex ? 3 : 2, e.size() - (hex ? 4 : 3)))
        {
            const int d = std::isdigit(static_cast<unsigned char>(c))
                ? c - '0'
                : (hex ? std::tolower(static_cast<unsigned char>(c)) - 'a' + 10 : -1);
            if(d < 0 || d >= (hex ? 16 : 10))
                return std::string(e);
            cp = std::min<std::uint32_t>(
                cp * (hex ? 16 : 10) + static_cast<std::uint32_t>(d), 0x110000);
        }
        append_utf8(out, cp);
        return out;
    }
    static constexpr std::pair<std::string_view, std::uint32_t> named[] = {{"amp", '&'},
        {"lt", '<'},
        {"gt", '>'},
        {"quot", '"'},
        {"apos", '\''},
        {"nbsp", 0xA0},
        {"copy", 0xA9},
        {"reg", 0xAE},
        {"trade", 0x2122},
        {"deg", 0xB0},
        {"times", 0xD7},
        {"divide", 0xF7},
        {"plusmn", 0xB1},
        {"micro", 0xB5},
        {"middot", 0xB7},
        {"sect", 0xA7},
        {"para", 0xB6},
        {"laquo", 0xAB},
        {"raquo", 0xBB},
        {"euro", 0x20AC},
        {"pound", 0xA3},
        {"yen", 0xA5},
        {"cent", 0xA2},
        {"ndash", 0x2013},
        {"mdash", 0x2014},
        {"lsquo", 0x2018},
        {"rsquo", 0x2019},
        {"ldquo", 0x201C},
        {"rdquo", 0x201D},
        {"hellip", 0x2026},
        {"bull", 0x2022},
        {"rarr", 0x2192},
        {"larr", 0x2190},
        {"check", 0x2713}};
    const std::string_view name = e.substr(1, e.size() - 2);
    for(const auto &[n, cp] : named)
        if(n == name)
        {
            append_utf8(out, cp);
            return out;
        }
    return std::string(e);
}

std::string decode(const MD_ATTRIBUTE &a)
{
    std::string out;
    for(unsigned i = 0; a.substr_offsets[i] < a.size; ++i)
    {
        const std::string_view s(
            a.text + a.substr_offsets[i], a.substr_offsets[i + 1] - a.substr_offsets[i]);
        switch(a.substr_types[i])
        {
        case MD_TEXT_ENTITY: out += decode_entity(s); break;
        case MD_TEXT_NULLCHAR: append_utf8(out, 0xFFFD); break;
        default: out += s;
        }
    }
    return out;
}

/// The value of attribute name in an HTML tag such as <a id="x">, or nothing.
std::optional<std::string> tag_attribute(std::string_view tag, std::string_view name)
{
    for(size_t pos = tag.find(name); pos != std::string_view::npos; pos = tag.find(name, pos + 1))
    {
        if(pos == 0 || !std::isspace(static_cast<unsigned char>(tag[pos - 1])))
            continue;
        size_t i = pos + name.size();
        while(i != tag.size() && tag[i] == ' ')
            ++i;
        if(i == tag.size() || tag[i] != '=')
            continue;
        ++i;
        while(i != tag.size() && tag[i] == ' ')
            ++i;
        if(i == tag.size())
            return std::nullopt;
        std::string raw;
        if(tag[i] == '"' || tag[i] == '\'')
        {
            const size_t end = tag.find(tag[i], i + 1);
            if(end == std::string_view::npos)
                return std::nullopt;
            raw = tag.substr(i + 1, end - i - 1);
        }
        else
        {
            const size_t end = tag.find_first_of(" />", i);
            raw = tag.substr(i, end == std::string_view::npos ? std::string_view::npos : end - i);
        }
        std::string out;
        for(size_t j = 0; j != raw.size(); ++j)
        {
            const size_t semi = raw[j] == '&' ? raw.find(';', j) : std::string::npos;
            if(semi == std::string::npos)
            {
                out += raw[j];
                continue;
            }
            out += decode_entity(std::string_view(raw).substr(j, semi - j + 1));
            j = semi;
        }
        return out;
    }
    return std::nullopt;
}

bool starts_with_tag(std::string_view html, std::string_view tag)
{
    if(html.size() < tag.size() + 1 || html[0] != '<')
        return false;
    for(size_t i = 0; i != tag.size(); ++i)
        if(std::tolower(static_cast<unsigned char>(html[i + 1])) != tag[i])
            return false;
    const char after = html.size() > tag.size() + 1 ? html[tag.size() + 1] : '>';
    return after == '>' || after == '/' || std::isspace(static_cast<unsigned char>(after));
}

const list_style_num *numbering(const list_style *ls, int level)
{
    if(!ls)
        return nullptr;
    for(const auto &s : ls->m_level_styles)
        if(std::visit([](const auto &x) { return x.m_level; }, s) == level)
            return std::get_if<list_style_num>(&s);
    return nullptr;
}

constexpr int list_levels = 10; // as many as ODF has

class builder
{
public:
    text_doc doc;

    builder() { m_blocks.push_back(&doc.body); }

    int enter_block(MD_BLOCKTYPE type, void *detail)
    {
        close_implicit();
        switch(type)
        {
        case MD_BLOCK_QUOTE: ++m_quote; break;
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
        {
            const bool ordered = type == MD_BLOCK_OL;
            const unsigned start = ordered ? static_cast<MD_BLOCK_OL_DETAIL *>(detail)->start : 1;
            list l;
            const std::string name = list_style_for(ordered, static_cast<int>(start));
            if(!name.empty())
                l.set_style(name);
            m_list_styles.push_back(name.empty() ? m_list_styles.back() : name);
            blocks().push_back(block_node{std::move(l)});
            m_lists.push_back(&std::get<list>(blocks().back().v));
            break;
        }
        case MD_BLOCK_LI:
            m_lists.back()->add(list_item{});
            m_blocks.push_back(&m_lists.back()->items().back().children());
            break;
        case MD_BLOCK_HR: blocks().push_back(block_node{page_break{}}); break;
        case MD_BLOCK_H:
        {
            blocks().push_back(block_node{
                heading(static_cast<int>(static_cast<MD_BLOCK_H_DETAIL *>(detail)->level))});
            m_inlines.push_back(&std::get<heading>(blocks().back().v).children());
            break;
        }
        case MD_BLOCK_CODE:
            m_code = true;
            m_code_text.clear();
            break;
        case MD_BLOCK_HTML: ++m_skip; break;
        case MD_BLOCK_P: open_paragraph(); break;
        case MD_BLOCK_TABLE:
            blocks().push_back(block_node{table{}});
            m_tables.push_back(&std::get<table>(blocks().back().v));
            m_tables.back()->set_header_rows(1);
            m_rows.emplace_back();
            break;
        case MD_BLOCK_TR: m_rows.back().clear(); break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
            m_rows.back().emplace_back();
            m_blocks.push_back(&m_rows.back().back().children());
            break;
        default: break; // DOC, THEAD, TBODY; extensions that aren't enabled
        }
        return 0;
    }

    int leave_block(MD_BLOCKTYPE type, void *)
    {
        close_implicit();
        switch(type)
        {
        case MD_BLOCK_QUOTE: --m_quote; break;
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            m_lists.pop_back();
            m_list_styles.pop_back();
            break;
        case MD_BLOCK_LI:
        case MD_BLOCK_TH:
        case MD_BLOCK_TD: m_blocks.pop_back(); break;
        case MD_BLOCK_TR:
        {
            // A row with fewer or more cells than the header is filled out or cut.
            table &t = *m_tables.back();
            auto &row = m_rows.back();
            if(t.num_rows() != 0)
                row.resize(t.num_cols());
            t.add_row(std::move(row));
            row.clear();
            break;
        }
        case MD_BLOCK_H: m_inlines.pop_back(); break;
        case MD_BLOCK_CODE:
        {
            m_code = false;
            // Each line ends with '\n', the last one included.
            if(m_code_text.ends_with('\n'))
                m_code_text.pop_back();
            paragraph p;
            if(!m_code_text.empty())
                p.add(text{m_code_text});
            blocks().push_back(block_node{p.set_style(builtin::preformatted)});
            break;
        }
        case MD_BLOCK_HTML: --m_skip; break;
        case MD_BLOCK_P: close_paragraph(); break;
        case MD_BLOCK_TABLE:
            m_rows.pop_back();
            finish_table();
            break;
        default: break;
        }
        return 0;
    }

    int enter_span(MD_SPANTYPE type, void *detail)
    {
        if(m_alt)
        {
            if(type == MD_SPAN_IMG)
                ++m_alt;
            return 0;
        }
        switch(type)
        {
        case MD_SPAN_EM: open_span(names::emphasis, text_props(font_style::italic)); break;
        case MD_SPAN_STRONG: open_span(names::strong, text_props(font_weight::bold)); break;
        case MD_SPAN_CODE:
        {
            inlines().push_back(inline_node{span().set_style(builtin::source_text)});
            m_inlines.push_back(&std::get<span>(inlines().back().v).children());
            break;
        }
        case MD_SPAN_A:
        {
            std::string href = decode(static_cast<MD_SPAN_A_DETAIL *>(detail)->href);
            hyperlink h = href.starts_with('#')
                ? hyperlink(link_target(bookmark_target{href.substr(1)}))
                : hyperlink(link_target(url_target{href}));
            inlines().push_back(inline_node{std::move(h)});
            m_inlines.push_back(&std::get<hyperlink>(inlines().back().v).children());
            break;
        }
        case MD_SPAN_IMG:
            inlines().push_back(
                inline_node{frame(image{decode(static_cast<MD_SPAN_IMG_DETAIL *>(detail)->src)})});
            m_alt = 1; // its text is the alt text, not content
            m_alt_frame = &std::get<frame>(inlines().back().v);
            break;
        default: m_inlines.push_back(&inlines()); break; // e.g. strikethrough: text only
        }
        return 0;
    }

    int leave_span(MD_SPANTYPE type, void *)
    {
        if(m_alt)
        {
            if(type == MD_SPAN_IMG && --m_alt == 0)
                m_alt_frame = nullptr;
            return 0;
        }
        m_inlines.pop_back();
        return 0;
    }

    int on_text(MD_TEXTTYPE type, std::string_view s)
    {
        if(m_alt && m_alt_frame && !m_skip)
        {
            m_alt_frame->accessibility_title += type == MD_TEXT_ENTITY ? decode_entity(s)
                : type == MD_TEXT_SOFTBR || type == MD_TEXT_BR       ? std::string(" ")
                                                                     : std::string(s);
            return 0;
        }
        if(m_skip || m_alt)
            return 0;
        if(m_code && type == MD_TEXT_CODE)
        {
            m_code_text += s;
            return 0;
        }
        switch(type)
        {
        case MD_TEXT_NULLCHAR: add_text("\xEF\xBF\xBD"); break;
        case MD_TEXT_BR: add_text("\n"); break;
        case MD_TEXT_SOFTBR: add_text(" "); break;
        case MD_TEXT_ENTITY: add_text(decode_entity(s)); break;
        case MD_TEXT_HTML: raw_html(s); break;
        default: add_text(s); break;
        }
        return 0;
    }

private:
    std::vector<block_node> &blocks() { return *m_blocks.back(); }
    std::vector<inline_node> &inlines()
    {
        if(m_inlines.empty())
        {
            // Tight list items and table cells hold inline content directly.
            open_paragraph();
            m_implicit = true;
        }
        return *m_inlines.back();
    }

    void open_paragraph()
    {
        paragraph p;
        if(m_quote)
        {
            doc.styles().add(style(names::quotations, style_family::paragraph));
            p.set_style(names::quotations);
        }
        blocks().push_back(block_node{std::move(p)});
        m_inlines.push_back(&std::get<paragraph>(blocks().back().v).children());
    }

    /// A paragraph that holds only an image is the image as a block.
    void close_paragraph()
    {
        m_inlines.pop_back();
        auto &p = std::get<paragraph>(blocks().back().v);
        if(p.children().size() == 1 && p.style.is_empty())
            if(const auto *f = std::get_if<frame>(&p.children().front().v))
                blocks().back() = block_node{frame(*f)};
    }

    void close_implicit()
    {
        if(!m_implicit)
            return;
        m_implicit = false;
        close_paragraph();
    }

    void open_span(const style_name &name, const text_props &props)
    {
        doc.styles().add(style(name, style_family::text, props));
        inlines().push_back(inline_node{span().set_style(name)});
        m_inlines.push_back(&std::get<span>(inlines().back().v).children());
    }

    void add_text(std::string_view s)
    {
        auto &kids = inlines();
        if(!kids.empty())
            if(auto *t = std::get_if<text>(&kids.back().v))
            {
                t->content += s;
                return;
            }
        kids.push_back(inline_node{text{std::string(s)}});
    }

    /// Raw inline HTML: an <a id> anchor is a bookmark and <br> a line break.
    /// Anything else has no place in the model.
    void raw_html(std::string_view tag)
    {
        if(starts_with_tag(tag, "br"))
            add_text("\n");
        else if(starts_with_tag(tag, "a"))
        {
            auto id = tag_attribute(tag, "id");
            if(!id)
                id = tag_attribute(tag, "name");
            if(id && !id->empty())
                inlines().push_back(inline_node{bookmark{*id}});
        }
    }

    /// The list style for a new list, or empty when it continues the enclosing
    /// list's style (same kind, same start) at its level.
    std::string list_style_for(bool ordered, int start)
    {
        const int level = static_cast<int>(m_lists.size()) + 1;
        if(!m_list_styles.empty())
        {
            const list_style *outer = doc.list_styles().find(m_list_styles.back());
            const auto *num = numbering(outer, level);
            if(outer && (ordered ? num && num->m_start_from == start : !num))
                return {};
        }
        std::string name = ordered ? names::numbered_list : names::bullet_list;
        if(ordered && start != 1)
            name += "_start_" + std::to_string(start);
        if(!doc.list_styles().find(name))
        {
            list_style ls(name, {});
            for(int lvl = 1; lvl <= list_levels; ++lvl)
                if(ordered)
                    ls.m_level_styles.emplace_back(
                        list_style_num(style{}, lvl, list_enum::arabic, ".", start));
                else
                    ls.m_level_styles.emplace_back(
                        list_style_bullet(style{}, lvl, bullet_type::bullet()));
            doc.list_styles().add(ls);
        }
        return name;
    }

    /// GFM tables always have a header row; an empty one means there was none.
    void finish_table()
    {
        table &t = *m_tables.back();
        m_tables.pop_back();
        if(t.num_rows() == 0)
            return;
        const auto header = t.cells().first(t.num_cols());
        const bool empty_header = std::all_of(header.begin(),
            header.end(),
            [](const table_cell &c)
            {
                return std::all_of(c.children().begin(),
                    c.children().end(),
                    [](const block_node &b)
                    {
                        const auto *p = std::get_if<paragraph>(&b.v);
                        return p && p->children().empty();
                    });
            });
        if(empty_header)
        {
            t.remove_row(0);
            t.set_header_rows(0);
        }
    }

    std::vector<std::vector<block_node> *> m_blocks;   //!< Where blocks go, innermost last
    std::vector<std::vector<inline_node> *> m_inlines; //!< Where inline content goes
    std::vector<list *> m_lists;
    std::vector<std::string> m_list_styles; //!< Each open list's effective list style
    std::vector<table *> m_tables;
    std::vector<std::vector<table_cell>> m_rows; //!< Each open table's row being read
    int m_quote{0};
    int m_skip{0}; //!< Inside an HTML block
    int m_alt{0};  //!< Inside an image's alt text
    frame *m_alt_frame = nullptr; //!< The image whose alt text it is
    bool m_code{false};
    bool m_implicit{false}; //!< The innermost paragraph was opened for bare inline content
    std::string m_code_text;
};

std::string media_type(const fs::path &p)
{
    std::string ext = p.extension().string();
    std::transform(
        ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    if(ext == ".png")
        return "image/png";
    if(ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if(ext == ".gif")
        return "image/gif";
    if(ext == ".svg")
        return "image/svg+xml";
    if(ext == ".webp")
        return "image/webp";
    if(ext == ".bmp")
        return "image/bmp";
    return "application/octet-stream";
}

} // namespace

text_doc parse(std::string_view markdown)
{
    builder b;
    MD_PARSER parser{};
    parser.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_PERMISSIVEAUTOLINKS;
    parser.enter_block = [](MD_BLOCKTYPE t, void *d, void *u)
    { return static_cast<builder *>(u)->enter_block(t, d); };
    parser.leave_block = [](MD_BLOCKTYPE t, void *d, void *u)
    { return static_cast<builder *>(u)->leave_block(t, d); };
    parser.enter_span = [](MD_SPANTYPE t, void *d, void *u)
    { return static_cast<builder *>(u)->enter_span(t, d); };
    parser.leave_span = [](MD_SPANTYPE t, void *d, void *u)
    { return static_cast<builder *>(u)->leave_span(t, d); };
    parser.text = [](MD_TEXTTYPE t, const MD_CHAR *s, MD_SIZE n, void *u)
    { return static_cast<builder *>(u)->on_text(t, std::string_view(s, n)); };

    if(md_parse(markdown.data(), static_cast<MD_SIZE>(markdown.size()), &parser, &b) != 0)
        throw std::runtime_error("Markdown could not be parsed");
    return std::move(b.doc);
}

file file::load(const fs::path &p)
{
    file f(parse(detail::read_bytes(p)));
    const fs::path base = p.parent_path();
    for(frame *fr : collect<frame>(f.doc))
    {
        const std::string &uri = fr->img.uri;
        const fs::path rel(std::u8string(uri.begin(), uri.end()));
        std::error_code ec;
        if(uri.empty() || uri.find(':') != std::string::npos || rel.is_absolute() ||
            f.doc.resources().find(uri) || !fs::is_regular_file(base / rel, ec))
            continue;
        f.doc.resources().add(resource(uri, detail::read_bytes(base / rel), media_type(rel)));
    }
    return f;
}

} // namespace docsmith::md
