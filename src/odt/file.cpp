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
#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/error.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <pugixml.hpp>

#include "odf/meta.h"
#include "odf/names.h"
#include "odf/package.h"
#include "odt/styles.h"
#include "odt/writer.h"
#include "titles.h"

namespace docsmith
{
namespace odt
{
namespace
{

/// Serialize an XML subtree verbatim so it can be re-emitted unchanged.
std::string serialize(const pugi::xml_node &node)
{
    std::ostringstream ss;
    node.print(ss, "", pugi::format_raw);
    return ss.str();
}

/// The model's name for the style a node names in attr.
std::string style_of(const pugi::xml_node &node, const char *attr)
{ return odf::model_name(node.attribute(attr).as_string()); }

/// A node's style, unstyled if it's the default the node has without one
/// (builtin::para, ...).
template <style_family F>
style_name<F> style_of(
    const pugi::xml_node &node, const char *attr, const style_name<F> &default_style)
{
    style_name<F> sn = style_of(node, attr);
    return sn == default_style ? style_name<F>{} : sn;
}

thread_local int t_list_depth = 0; //!< Lists the reader is in

std::vector<inline_node> parse_inline_children(const pugi::xml_node &node);
std::vector<block_node> parse_block_children(const pugi::xml_node &node);
std::optional<block_node> parse_block(const pugi::xml_node &node);

image parse_image(const pugi::xml_node &node) { return image{node.attribute("xlink:href").as_string()}; }

frame parse_frame(const pugi::xml_node &node)
{
    frame f;
    f.style = style_of(node, "draw:style-name");
    f.width = length_from(node.attribute("svg:width").as_string());
    f.height = length_from(node.attribute("svg:height").as_string());
    if(auto img = node.child("draw:image"))
        f.img = parse_image(img);
    f.accessibility_title = node.child("svg:title").text().as_string();
    f.accessibility_description = node.child("svg:desc").text().as_string();
    return f;
}

inline_node parse_inline(const pugi::xml_node &node)
{
    std::string tag = node.name();

    if(tag == "text:span")
    {
        span s;
        s.style = style_of(node, "text:style-name");
        s.items() = parse_inline_children(node);
        return inline_node{std::move(s)};
    }
    if(tag == "text:a")
    {
        const std::string href = node.attribute("xlink:href").as_string();
        hyperlink h = href.starts_with('#') ? hyperlink(bookmark_target{href.substr(1)})
                                            : hyperlink(url_target{href});
        h.style = style_of(node, "text:style-name", builtin::link); // its visited style is dropped
        h.items() = parse_inline_children(node);
        return inline_node{std::move(h)};
    }
    if(tag == "draw:frame")
        return inline_node{parse_frame(node)};
    if(tag == "text:bookmark" || tag == "text:bookmark-start")
        return inline_node{bookmark{node.attribute("text:name").as_string()}};
    if(tag == "text:page-number")
        return inline_node{field{field_type::page_number}};
    if(tag == "text:page-count")
        return inline_node{field{field_type::page_count}};
    if(tag == "text:date")
        return inline_node{field{field_type::date}};
    if(tag == "text:title")
        return inline_node{field{field_type::title}};

    return inline_node{unknown{serialize(node)}};
}

/// Appends s to the last node if that is text, so that ODF's space, tab and
/// line break elements join the text around them.
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

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

/// Text as ODF reads it: each run of white space is one space, and white
/// space at the start of a paragraph or heading is dropped.
std::string collapse_space(std::string_view s, bool at_start)
{
    std::string out;
    bool space = at_start;
    for(char c : s)
    {
        if(is_space(c))
        {
            if(!space)
                out += ' ';
            space = true;
        }
        else
        {
            out += c;
            space = false;
        }
    }
    return out;
}

/// Markup that only caches LibreOffice's last layout: soft page breaks and a
/// list item's label as shown (text:number). It is dropped when read, since
/// it would go stale once the document is edited, and LibreOffice makes it
/// again when it lays the document out.
bool is_layout_cache(std::string_view tag)
{ return tag == "text:soft-page-break" || tag == "text:number"; }

std::vector<inline_node> parse_inline_children(const pugi::xml_node &node)
{
    const std::string_view tag = node.name();
    const bool paragraph = tag == "text:p" || tag == "text:h";

    std::vector<inline_node> out;
    for(auto child : node.children())
    {
        const std::string_view name = child.name();
        if(child.type() == pugi::node_pcdata)
        {
            const bool at_start = paragraph && child == node.first_child();
            if(auto s = collapse_space(child.value(), at_start); !s.empty())
                append_text(out, s);
        }
        else if(child.type() != pugi::node_element || is_layout_cache(name))
            continue;
        else if(name == "text:s")
            append_text(out, std::string(child.attribute("text:c").as_uint(1), ' '));
        else if(name == "text:tab")
            append_text(out, "\t");
        else if(name == "text:line-break")
            append_text(out, "\n");
        else
            out.push_back(parse_inline(child));
    }
    return out;
}

/// How many times an element stands for itself (table:number-*-repeated).
unsigned repeats(const pugi::xml_node &n, const char *attr)
{ return std::max(1U, n.attribute(attr).as_uint(1)); }

std::vector<table_cell> parse_row(const pugi::xml_node &row_node)
{
    std::vector<table_cell> row;
    for(auto cell_node : row_node.children())
    {
        std::string ct = cell_node.name();
        const unsigned n = repeats(cell_node, "table:number-columns-repeated");
        if(ct == "table:table-cell")
        {
            table_cell c;
            c.rowspan = cell_node.attribute("table:number-rows-spanned").as_int(1);
            c.colspan = cell_node.attribute("table:number-columns-spanned").as_int(1);
            c.style = style_of(cell_node, "table:style-name");
            c.items() = parse_block_children(cell_node);
            row.insert(row.end(), n, c);
        }
        else if(ct == "table:covered-table-cell")
            row.insert(row.end(), n, table_cell{}); // placeholders for spanned-over slots
    }
    return row;
}

/// Column widths by the name of the automatic column style that holds them,
/// as LibreOffice gives every table; set while content.xml is read.
thread_local const std::map<std::string, double> *t_column_widths = nullptr;

struct column
{
    std::optional<double> width;
    cell_style_name cell_style; //!< table:default-cell-style-name
};

/// Each column, in order, from table:table-column elements (also in column
/// groups). The column's own style is only read for its width.
void parse_columns(const pugi::xml_node &node, std::vector<column> &out)
{
    for(auto child : node.children())
    {
        const std::string_view tag = child.name();
        if(tag == "table:table-column")
        {
            column c{.cell_style = style_of(child, "table:default-cell-style-name")};
            if(t_column_widths)
                if(auto it = t_column_widths->find(child.attribute("table:style-name").as_string());
                    it != t_column_widths->end())
                    c.width = it->second;
            out.insert(out.end(), repeats(child, "table:number-columns-repeated"), c);
        }
        else if(tag == "table:table-columns" || tag == "table:table-header-columns" ||
            tag == "table:table-column-group")
            parse_columns(child, out);
    }
}

table parse_table(const pugi::xml_node &node)
{
    struct row
    {
        std::vector<table_cell> cells;
        row_style_name style;
    };
    std::vector<row> rows;
    size_t header_rows = 0;
    auto add = [&rows](const pugi::xml_node &r)
    {
        for(unsigned i = repeats(r, "table:number-rows-repeated"); i != 0; --i)
            rows.push_back({parse_row(r), style_of(r, "table:style-name")});
    };
    for(auto child : node.children())
    {
        std::string tag = child.name();
        if(tag == "table:table-header-rows")
        {
            const bool at_top = rows.empty();
            for(auto r : child.children("table:table-row"))
                add(r);
            // The model has header rows only at the top; later ones are ordinary.
            if(at_top)
                header_rows = rows.size();
        }
        else if(tag == "table:table-row")
            add(child);
        else if(tag == "table:table-rows")
            for(auto r : child.children("table:table-row"))
                add(r);
    }

    // A ragged table is filled out to its widest row.
    size_t cols = 0;
    for(const auto &r : rows)
        cols = std::max(cols, r.cells.size());
    table t;
    for(auto &r : rows)
    {
        r.cells.resize(cols);
        t.add_row(std::move(r.cells), std::move(r.style));
    }
    t.set_header_rows(header_rows);
    t.style = style_of(node, "table:style-name", builtin::table);
    t.name = node.attribute("table:name").as_string();
    t.accessibility_title = node.child("table:title").text().as_string();
    t.accessibility_description = node.child("table:desc").text().as_string();

    std::vector<column> columns;
    parse_columns(node, columns);
    if(columns.size() == t.num_cols())
        for(size_t c = 0; c != cols; ++c)
        {
            t.set_column_width(c, columns[c].width).set_column_style(c, columns[c].cell_style);
            // The writer writes the column's style on its cells as well.
            for(size_t r = 0; r != t.num_rows(); ++r)
                if(auto &cell = t.cell(r, c); cell.style == columns[c].cell_style)
                    cell.style = {};
        }
    return t;
}

list parse_list(const pugi::xml_node &node)
{
    list l;
    l.style = t_list_depth == 0 ? style_of(node, "text:style-name", builtin::list)
                                : list_style_name(style_of(node, "text:style-name"));
    struct depth_guard
    {
        depth_guard() { ++t_list_depth; }
        ~depth_guard() { --t_list_depth; }
        depth_guard(const depth_guard &) = delete;
        depth_guard &operator=(const depth_guard &) = delete;
    } guard;
    // text:continue-list names the list it continues: usually the last one
    // before with the same style, which is what continue_numbering means.
    l.continue_numbering =
        node.attribute("text:continue-numbering").as_bool() || node.attribute("text:continue-list");
    for(auto item_node : node.children())
    {
        const std::string_view tag = item_node.name();
        if(tag != "text:list-item" && tag != "text:list-header")
            continue;
        list_item it;
        it.numbered = tag == "text:list-item";
        if(auto v = item_node.attribute("text:start-value"); v && it.numbered)
            it.start_value = v.as_int();
        it.items() = parse_block_children(item_node);
        l.items().push_back(std::move(it));
    }
    return l;
}

std::optional<block_node> parse_block(const pugi::xml_node &node)
{
    std::string tag = node.name();

    if(is_layout_cache(tag))
        return std::nullopt;
    if(tag == "text:p")
    {
        paragraph p;
        p.style = style_of(node, "text:style-name", builtin::para);
        p.items() = parse_inline_children(node);
        // The writer puts a block frame in an unstyled paragraph of its own.
        if(!node.attribute("text:style-name") && p.items().size() == 1 &&
            std::holds_alternative<frame>(p.items().front().v))
            return block_node{std::get<frame>(p.items().front().v)};
        return block_node{std::move(p)};
    }
    if(tag == "text:h")
    {
        heading h(node.attribute("text:outline-level").as_int(1));
        h.style = style_of(node, "text:style-name", builtin::heading(h.level));
        h.items() = parse_inline_children(node);
        return block_node{std::move(h)};
    }
    if(tag == "text:list")
        return block_node{parse_list(node)};
    if(tag == "table:table")
        return block_node{parse_table(node)};
    if(tag == "draw:frame")
        return block_node{parse_frame(node)};
    if(tag == "text:table-of-content")
    {
        // The entries are left out: the writer makes them from the headings.
        auto source = node.child("text:table-of-content-source");
        return block_node{
            table_of_contents(source.child("text:index-title-template").text().as_string(),
                source.attribute("text:outline-level").as_int(10))};
    }

    return block_node{unknown{serialize(node)}};
}

std::vector<block_node> parse_block_children(const pugi::xml_node &node)
{
    std::vector<block_node> out;
    for(auto child : node.children())
        if(child.type() == pugi::node_element)
            if(auto b = parse_block(child))
                out.push_back(std::move(*b));
    return out;
}

/// Undoes the writer's folding of page_break nodes into the next paragraph or
/// heading (see writer::page_break_style()). styles: the page break styles in
/// content.xml, and the style each was made from.
void expand_page_breaks(
    std::vector<block_node> &blocks, const std::map<std::string, para_style_name> &styles)
{
    std::vector<block_node> out;
    for(auto &b : blocks)
    {
        bool drop = false;
        std::visit(
            overloaded{
                [&](list &l)
                {
                    for(auto &it : l.items())
                        expand_page_breaks(it.items(), styles);
                    // The writer folds a break before a list into its
                    // first paragraph; it reads back before the list.
                    if(auto &items = l.items(); !items.empty() && !items.front().items().empty() &&
                        std::holds_alternative<page_break>(items.front().items().front().v))
                    {
                        items.front().items().erase(items.front().items().begin());
                        out.push_back(block_node{page_break{}});
                    }
                },
                [&](table &t)
                {
                    for(auto &c : t.cells())
                        expand_page_breaks(c.items(), styles);
                },
                [&](auto &n)
                {
                    using T = std::decay_t<decltype(n)>;
                    if constexpr(std::is_same_v<T, paragraph> || std::is_same_v<T, heading>)
                    {
                        auto it = styles.find(n.style.str());
                        if(it == styles.end())
                            return;
                        out.push_back(block_node{page_break{}});
                        drop = it->first == page_break_only;
                        n.style = it->second;
                        if constexpr(std::is_same_v<T, paragraph>)
                        {
                            if(n.style == builtin::para)
                                n.style = {};
                        }
                        else if(n.style == builtin::heading(n.level))
                            n.style = {};
                    }
                },
            },
            b.v);
        if(!drop)
            out.push_back(std::move(b));
    }
    blocks = std::move(out);
}

/// The header and footer of the master page the body uses: "Standard"
/// (LibreOffice's Default Page Style), else the first one.
/// Column widths by style name, from the automatic column styles.
std::map<std::string, double> column_widths(const pugi::xml_node &automatic_styles)
{
    std::map<std::string, double> widths;
    for(auto s : automatic_styles.children("style:style"))
        if(std::string_view(s.attribute("style:family").as_string()) == "table-column")
            if(auto w = read_column_width(s))
                widths[s.attribute("style:name").as_string()] = *w;
    return widths;
}

page_style parse_page(const odf::package &pkg)
{
    page_style page;
    const auto *e = pkg.find("styles.xml");
    pugi::xml_document xml;
    if(!e ||
        !xml.load_buffer(
            e->data.data(), e->data.size(), pugi::parse_default | pugi::parse_ws_pcdata))
        return page;
    auto masters = xml.child("office:document-styles").child("office:master-styles");
    auto master = masters.find_child_by_attribute("style:master-page", "style:name", "Standard");
    if(!master)
        master = masters.child("style:master-page");
    page.header = parse_block_children(master.child("style:header"));
    page.footer = parse_block_children(master.child("style:footer"));
    return page;
}

constexpr const char *odt_mimetype = "application/vnd.oasis.opendocument.text";

} // namespace

struct file::impl
{
    odf::package pkg; //!< Everything in the archive; empty for a new document
};

file::file() :
    m_impl(impl{odf::package(odt_mimetype)})
{
}

file::file(text_doc d) :
    doc(std::move(d)), m_impl(impl{odf::package(odt_mimetype)})
{
}

file file::load(const std::filesystem::path &p)
{
    file f;
    f.m_impl->pkg = odf::package::read(p);

    // Text documents and their relatives (.ott templates) share this prefix.
    if(!f.m_impl->pkg.mimetype().starts_with(odt_mimetype))
        throw error(error_code::not_a_document, "Not an OpenDocument text file: " + p.string());

    const auto *content = f.m_impl->pkg.find("content.xml");
    pugi::xml_document xml;
    // White space between inline elements is a space, so keep it.
    if(!content ||
        !xml.load_buffer(content->data.data(),
            content->data.size(),
            pugi::parse_default | pugi::parse_ws_pcdata))
        throw error(error_code::malformed, "Could not read content.xml from " + p.string());

    auto root = xml.child("office:document-content");
    const auto widths = column_widths(root.child("office:automatic-styles"));
    {
        struct reading
        {
            explicit reading(const std::map<std::string, double> &w) { t_column_widths = &w; }
            ~reading() { t_column_widths = nullptr; }
        } guard(widths);
        f.doc.body = parse_block_children(root.child("office:body").child("office:text"));
    }

    std::map<std::string, para_style_name> page_breaks;
    for(auto s : root.child("office:automatic-styles").children("style:style"))
        if(auto base = page_break_base(s))
            page_breaks[odf::model_name(s.attribute("style:name").as_string())] = *base;
    if(!page_breaks.empty())
        expand_page_breaks(f.doc.body, page_breaks);
    read_styles(f.m_impl->pkg, f.doc);
    f.doc.page = parse_page(f.m_impl->pkg);
    detail::read_titles(f.doc);
    if(const auto *meta = f.m_impl->pkg.find("meta.xml"))
        f.doc.meta = odf::read_meta(meta->data);

    // Move the images the tree refers to out of the package into the document,
    // so every format can reach them. Entries only unknown markup refers to
    // stay in the package and are carried over.
    for(const frame *fr : collect<frame>(f.doc))
        if(auto e = f.m_impl->pkg.take(fr->img.uri))
            f.doc.resources().insert_or_assign(
                resource(fr->img.uri, std::move(e->data), std::move(e->media_type)));
    return f;
}

namespace
{
/// Writes doc into out (a copy, so saving leaves the file it came from
/// unchanged and can be repeated) and out to p.
void write_package(const text_doc &doc, odf::package out, const std::filesystem::path &p)
{
    out.set_mimetype(odt_mimetype);

    write_document(doc, out);
    odf::write_meta(doc.meta, out);
    out.write(p);

#ifdef DOCSMITHCPP_DEBUG_FILES
    // Debug aid: content.xml and styles.xml next to the archive, as
    // <name>.content.xml and <name>.styles.xml.
    // Indented for reading; the package's copies aren't, as indentation between
    // inline elements would read as spaces.
    for(const char *part : {"content", "styles"})
    {
        const auto &data = out.find(std::string(part) + ".xml")->data;
        pugi::xml_document xml;
        xml.load_buffer(data.data(), data.size(), pugi::parse_default | pugi::parse_ws_pcdata);
        xml.save_file(
            (std::filesystem::path(p).replace_extension(std::string(".") + part + ".xml")).c_str(),
            "  ");
    }
#endif
}
} // namespace

void file::save(const std::filesystem::path &p) const { write_package(doc, m_impl->pkg, p); }

} // namespace odt
} // namespace docsmith
