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
#pragma once

// Styles: the property types, the style type of each family, and the style
// sheet a document keeps them in.

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "docsmithcpp/named_registry.h"

namespace docsmith
{

/// What kind of node a style applies to. Each family has its own style type
/// and its own names: a paragraph style and a text style may share a name.
enum class style_family
{
    para,  //!< paragraph, heading: paragraph_style
    text,  //!< span, hyperlink: text_style
    table, //!< table: table_style
    cell,  //!< table_cell, and a table's columns: cell_style
    row,   //!< a table's rows: row_style
    frame, //!< frame: frame_style
    list,  //!< list: list_style
};

/// The name of a style of family F. A string converts to it, so a node's
/// set_style("Note") names a style of the node's family.
template <style_family F>
class style_name
{
public:
    static constexpr style_family family = F;

    style_name() = default;
    style_name(const char *name) :
        m_name(name)
    {
    }
    style_name(std::string name) :
        m_name(std::move(name))
    {
    }

    const std::string &str() const { return m_name; }
    bool empty() const { return m_name.empty(); }

    bool operator==(const style_name &) const = default;
    friend bool operator<(const style_name &a, const style_name &b) { return a.m_name < b.m_name; }

private:
    std::string m_name;
};

using para_style_name = style_name<style_family::para>;
using text_style_name = style_name<style_family::text>;
using table_style_name = style_name<style_family::table>;
using cell_style_name = style_name<style_family::cell>;
using row_style_name = style_name<style_family::row>;
using frame_style_name = style_name<style_family::frame>;
using list_style_name = style_name<style_family::list>;

/// Style names writers know without a definition in the document's styles().
/// Each writer maps them to its format's own style of that name.
///
/// para, heading(n), link, table and list are the defaults: the styles of
/// nodes with none of their own. An unstyled paragraph has builtin::para, and
/// so on. Each is a definable style like any other. Undefined, the format's
/// own default applies; defined (in code, or by the template loaded), it
/// applies in every format. Reading a file gives its defaults as these
/// definitions and the nodes using them as unstyled.
namespace builtin
{
/// Paragraphs: LibreOffice's Default Paragraph Style, Word's Normal, and a
/// plain <p> in HTML. Other paragraph styles, headings' included, usually
/// derive from it, so a font set here applies to them too.
inline const para_style_name para{"Standard"};
/// Headings of a level (1-10): LibreOffice's and Word's Heading 1, ..., and
/// <h1>, ... in HTML.
inline para_style_name heading(int level)
{ return para_style_name("Heading_20_" + std::to_string(std::clamp(level, 1, 10))); }
/// Links: LibreOffice's Internet Link, Word's Hyperlink, and a plain <a> in
/// HTML.
inline const text_style_name link{"Internet_20_link"};
/// Tables: Word's Normal Table. ODF has none of its own, so an ODT file names
/// it on its tables.
inline const table_style_name table{"Default_20_Table"};
/// Lists: a list with no style that isn't nested in another (a nested one
/// continues the enclosing list's style).
inline const list_style_name list{"Default_20_List"};

/// A title paragraph (text_doc::set_title()).
inline const para_style_name title{"Title"};
/// A monospace paragraph for code and logs, with its lines separated by '\n':
/// LibreOffice's Preformatted Text, and <pre> in HTML.
inline const para_style_name preformatted{"Preformatted_20_Text"};
/// Monospace text within a line: LibreOffice's Source Text, and <code> in HTML.
inline const text_style_name source_text{"Source_20_Text"};
} // namespace builtin

/// A font family, e.g. "Liberation Sans". ODT writes it as a font declaration
/// of the same name, and reads a declaration's family.
struct font_name
{
public:
    explicit font_name(const std::string &font_name) :
        m_font_name{font_name}
    {
    }
    const std::string &get_name() const { return m_font_name; }
    bool operator==(const font_name &rhs) const = default;

private:
    std::string m_font_name;
};

enum class font_style
{
    normal,
    italic,
    oblique
};
enum class font_weight
{
    normal,
    bold
};

struct font_size
{
    explicit font_size(float points) :
        m_points{points}
    {
    }
    bool operator==(const font_size &rhs) const = default;
    float m_points;
};

/// An sRGB text colour.
struct colour
{
    static constexpr colour rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b)
    { return {r, g, b}; }
    bool operator==(const colour &rhs) const = default;

    std::uint8_t m_r{};
    std::uint8_t m_g{};
    std::uint8_t m_b{};
};

/// A length, held in points. Writers convert it to their format's unit.
struct length
{
    double m_points{};
    bool operator==(const length &rhs) const = default;
};
constexpr length pt(double v) { return {v}; }
constexpr length mm(double v) { return {v * 72.0 / 25.4}; }
constexpr length cm(double v) { return {v * 72.0 / 2.54}; }
constexpr length inch(double v) { return {v * 72.0}; }

/// A share of the text width, e.g. a table's width.
struct percent
{
    double m_value{};
    bool operator==(const percent &rhs) const = default;
};

struct text_props
{
    text_props() = default;
    template <typename... Args>
        requires(sizeof...(Args) >= 1) && (!std::is_same_v<std::decay_t<Args>, text_props> && ...)
    explicit text_props(Args &&...args)
    {
        (set(args), ...);
    }

    text_props &set(const font_size &fs)
    {
        m_font_size = fs;
        return *this;
    }
    text_props &set(const font_style &fs)
    {
        m_font_style = fs;
        return *this;
    }
    text_props &set(const font_name &fn)
    {
        m_font_name = fn;
        return *this;
    }
    text_props &set(const font_weight &fw)
    {
        m_font_weight = fw;
        return *this;
    }
    text_props &set(const colour &c)
    {
        m_colour = c;
        return *this;
    }
    /// Sets each property over holds, keeping the others.
    text_props &overlay(const text_props &over)
    {
        auto take = [](auto &into, const auto &from)
        {
            if(from)
                into = from;
        };
        take(m_font_size, over.m_font_size);
        take(m_font_style, over.m_font_style);
        take(m_font_name, over.m_font_name);
        take(m_font_weight, over.m_font_weight);
        take(m_colour, over.m_colour);
        return *this;
    }
    bool empty() const { return *this == text_props{}; }
    bool operator==(const text_props &rhs) const = default;

    std::optional<font_size> m_font_size;
    std::optional<font_style> m_font_style;
    std::optional<font_name> m_font_name;
    std::optional<font_weight> m_font_weight;
    std::optional<colour> m_colour;
};

/// How a paragraph's lines line up. start and end are left and right in
/// left-to-right text; DOCX writes them as left and right.
enum class text_align
{
    start,
    end,
    left,
    right,
    centre,
    justify
};

enum class break_type
{
    column,
    page,
    even_page,
    odd_page,
    automatic
};

// clang-format off
struct break_before { break_type m_break_type; bool operator==(const break_before&) const = default; };
struct break_after { break_type m_break_type; bool operator==(const break_after&) const = default; };
/// Space above and below a paragraph.
struct space_before { length m_length; bool operator==(const space_before&) const = default; };
struct space_after { length m_length; bool operator==(const space_after&) const = default; };
/// Indents from the start (left in left-to-right text) and end edges.
struct indent_start { length m_length; bool operator==(const indent_start&) const = default; };
struct indent_end { length m_length; bool operator==(const indent_end&) const = default; };
/// The first line's indent beyond indent_start; negative for a hanging indent.
struct first_line_indent { length m_length; bool operator==(const first_line_indent&) const = default; };
/// Keep the paragraph on the same page as the next one (e.g. a heading).
struct keep_with_next { bool m_on{true}; bool operator==(const keep_with_next&) const = default; };
/// Don't split the paragraph across pages.
struct keep_together { bool m_on{true}; bool operator==(const keep_together&) const = default; };
// clang-format on

class paragraph_props
{
public:
    paragraph_props() = default;
    template <typename... Args>
        requires(sizeof...(Args) >= 1) &&
        (!std::is_same_v<std::decay_t<Args>, paragraph_props> && ...)
    explicit paragraph_props(Args &&...args)
    {
        (set(std::forward<Args>(args)), ...);
    }

    paragraph_props &set(const break_before &b)
    {
        m_break_before = b;
        return *this;
    }
    paragraph_props &set(const break_after &b)
    {
        m_break_after = b;
        return *this;
    }
    paragraph_props &set(text_align a)
    {
        m_align = a;
        return *this;
    }
    paragraph_props &set(const space_before &s)
    {
        m_space_before = s.m_length;
        return *this;
    }
    paragraph_props &set(const space_after &s)
    {
        m_space_after = s.m_length;
        return *this;
    }
    paragraph_props &set(const indent_start &i)
    {
        m_indent_start = i.m_length;
        return *this;
    }
    paragraph_props &set(const indent_end &i)
    {
        m_indent_end = i.m_length;
        return *this;
    }
    paragraph_props &set(const first_line_indent &i)
    {
        m_first_line_indent = i.m_length;
        return *this;
    }
    paragraph_props &set(const keep_with_next &k)
    {
        m_keep_with_next = k.m_on;
        return *this;
    }
    paragraph_props &set(const keep_together &k)
    {
        m_keep_together = k.m_on;
        return *this;
    }
    /// Sets each property over holds, keeping the others.
    paragraph_props &overlay(const paragraph_props &over);
    bool empty() const { return *this == paragraph_props{}; }
    bool operator==(const paragraph_props &rhs) const = default;

    std::optional<break_before> m_break_before;
    std::optional<break_after> m_break_after;
    std::optional<text_align> m_align;
    std::optional<length> m_space_before;
    std::optional<length> m_space_after;
    std::optional<length> m_indent_start;
    std::optional<length> m_indent_end;
    std::optional<length> m_first_line_indent;
    std::optional<bool> m_keep_with_next;
    std::optional<bool> m_keep_together;
};

inline paragraph_props &paragraph_props::overlay(const paragraph_props &over)
{
    auto take = [](auto &into, const auto &from)
    {
        if(from)
            into = from;
    };
    take(m_break_before, over.m_break_before);
    take(m_break_after, over.m_break_after);
    take(m_align, over.m_align);
    take(m_space_before, over.m_space_before);
    take(m_space_after, over.m_space_after);
    take(m_indent_start, over.m_indent_start);
    take(m_indent_end, over.m_indent_end);
    take(m_first_line_indent, over.m_first_line_indent);
    take(m_keep_with_next, over.m_keep_with_next);
    take(m_keep_together, over.m_keep_together);
    return *this;
}

enum class border_line
{
    none, //!< no line, e.g. to remove one the table's style gives a cell
    solid,
    dashed,
    dotted,
    double_line
};

/// One edge of a cell.
struct border
{
    static border solid(length width = pt(0.5), colour c = {})
    { return {border_line::solid, width, c}; }
    static border none() { return {border_line::none, {}, {}}; }
    bool operator==(const border &rhs) const = default;

    border_line m_line{border_line::solid};
    length m_width{0.5};
    colour m_colour{};
};

/// A cell's four edges. start and end are left and right in left-to-right text.
struct cell_borders
{
    bool operator==(const cell_borders &rhs) const = default;

    std::optional<border> m_top;
    std::optional<border> m_bottom;
    std::optional<border> m_start;
    std::optional<border> m_end;
};

/// The space between a cell's edges and its content, on every side.
struct cell_padding
{
    length m_length;
    bool operator==(const cell_padding &rhs) const = default;
};

enum class cell_valign
{
    top,
    middle,
    bottom
};

/// How a table cell looks: in a table_cell style, as a cell's direct
/// formatting (table_cell::props), or in a table style for its header rows
/// (header_cells). See DESIGN.md, Tables.
class cell_props
{
public:
    cell_props() = default;
    template <typename... Args>
        requires(sizeof...(Args) >= 1) && (!std::is_same_v<std::decay_t<Args>, cell_props> && ...)
    explicit cell_props(Args &&...args)
    { (set(args), ...); }

    cell_props &set(const cell_borders &b)
    {
        m_borders = b;
        return *this;
    }
    /// The background.
    cell_props &set(const colour &c)
    {
        m_background = c;
        return *this;
    }
    cell_props &set(cell_valign v)
    {
        m_valign = v;
        return *this;
    }
    cell_props &set(const cell_padding &p)
    {
        m_padding = p.m_length;
        return *this;
    }
    /// The paragraph style for the cell's paragraphs that have no style of
    /// their own.
    cell_props &set(const para_style_name &p)
    {
        m_paragraph_style = p;
        return *this;
    }
    /// Sets each property over holds, keeping the others.
    cell_props &overlay(const cell_props &over);
    bool empty() const { return *this == cell_props{}; }
    bool operator==(const cell_props &rhs) const = default;

    cell_borders m_borders;
    std::optional<colour> m_background;
    std::optional<cell_valign> m_valign;
    std::optional<length> m_padding;
    /// For the cell's paragraphs that have no style of their own; empty for
    /// none. Writers put it on those paragraphs: no format holds it in a
    /// cell style.
    para_style_name m_paragraph_style;
};

inline cell_props &cell_props::overlay(const cell_props &over)
{
    auto take = [](auto &into, const auto &from)
    {
        if(from)
            into = from;
    };
    take(m_borders.m_top, over.m_borders.m_top);
    take(m_borders.m_bottom, over.m_borders.m_bottom);
    take(m_borders.m_start, over.m_borders.m_start);
    take(m_borders.m_end, over.m_borders.m_end);
    take(m_background, over.m_background);
    take(m_valign, over.m_valign);
    take(m_padding, over.m_padding);
    if(!over.m_paragraph_style.empty())
        m_paragraph_style = over.m_paragraph_style;
    return *this;
}

enum class table_align
{
    start,
    centre,
    end
};

// clang-format off
/// The same line on all four edges of the table.
struct outer_border { border m_border; bool operator==(const outer_border&) const = default; };
/// The lines between rows.
struct inside_h { border m_border; bool operator==(const inside_h&) const = default; };
/// The lines between columns.
struct inside_v { border m_border; bool operator==(const inside_v&) const = default; };
// clang-format on

/// How the cells of a table's header rows (table::header_rows()) look, over
/// the table's lines and its columns' styles, under each cell's own style.
struct header_cells
{
    cell_props m_props;
    bool operator==(const header_cells &rhs) const = default;
};

/// How a table looks, in a table_style.
class table_props
{
public:
    table_props() = default;
    template <typename... Args>
        requires(sizeof...(Args) >= 1) && (!std::is_same_v<std::decay_t<Args>, table_props> && ...)
    explicit table_props(Args &&...args)
    { (set(args), ...); }

    table_props &set(const length &width)
    {
        m_width = width;
        return *this;
    }
    table_props &set(const percent &width)
    {
        m_width = width;
        return *this;
    }
    table_props &set(table_align a)
    {
        m_align = a;
        return *this;
    }
    table_props &set(const cell_borders &outer)
    {
        m_outer = outer;
        return *this;
    }
    table_props &set(const outer_border &b)
    {
        m_outer = {b.m_border, b.m_border, b.m_border, b.m_border};
        return *this;
    }
    table_props &set(const inside_h &b)
    {
        m_inside_h = b.m_border;
        return *this;
    }
    table_props &set(const inside_v &b)
    {
        m_inside_v = b.m_border;
        return *this;
    }
    table_props &set(const cell_padding &p)
    {
        m_padding = p.m_length;
        return *this;
    }
    table_props &set(const header_cells &h)
    {
        m_header = h.m_props;
        return *this;
    }
    bool operator==(const table_props &rhs) const = default;

    std::optional<std::variant<length, percent>> m_width;
    std::optional<table_align> m_align;
    cell_borders m_outer;
    std::optional<border> m_inside_h;
    std::optional<border> m_inside_v;
    std::optional<length> m_padding; //!< Every cell's, unless it sets its own
    cell_props m_header; //!< The header rows' cells (header_cells)

    bool empty() const { return *this == table_props{}; }
};

/// What every style has: its name, which is fixed, as it is the key the style
/// is found by in its family's registry.
template <style_family F>
class style_base
{
public:
    static constexpr style_family family = F;
    using name_type = style_name<F>;

    const name_type &name() const { return m_name; }

    bool operator==(const style_base &) const = default;

protected:
    explicit style_base(name_type name) :
        m_name(std::move(name))
    {
    }

private:
    name_type m_name;
};

/// A paragraph style, for paragraphs and headings. Its properties are given in
/// any order:
///
///     paragraph_style("Note", paragraph_props(space_after{pt(6)}), text_props(font_size(9)))
class paragraph_style : public style_base<style_family::para>
{
public:
    template <typename... Props>
    explicit paragraph_style(name_type name, const Props &...props) :
        style_base(std::move(name))
    { (assign(props), ...); }

    name_type parent; //!< The style it inherits from; empty for none
    paragraph_props para;
    text_props text;

    bool operator==(const paragraph_style &) const = default;

private:
    void assign(const paragraph_props &p) { para = p; }
    void assign(const text_props &t) { text = t; }
};

/// A text style, for spans and links.
class text_style : public style_base<style_family::text>
{
public:
    explicit text_style(name_type name, const text_props &props = {}) :
        style_base(std::move(name)), text(props)
    {
    }

    name_type parent; //!< The style it inherits from; empty for none
    text_props text;

    bool operator==(const text_style &) const = default;
};

/// How a table looks. The lines and the header apply to cells by their
/// position when the document is written, under each cell's own style and
/// direct formatting. See DESIGN.md, Tables.
class table_style : public style_base<style_family::table>
{
public:
    explicit table_style(name_type name, const table_props &props = {}) :
        style_base(std::move(name)), table(props)
    {
    }

    name_type parent; //!< The style it inherits from; empty for none
    table_props table;

    bool operator==(const table_style &) const = default;
};

/// How a table cell looks, for cells and a table's columns. para and text are
/// for the cell's paragraphs that have no style of their own, over
/// cell.m_paragraph_style: writers give those paragraphs a paragraph style
/// made from them, as LibreOffice ignores them in a cell style.
class cell_style : public style_base<style_family::cell>
{
public:
    template <typename... Props>
    explicit cell_style(name_type name, const Props &...props) :
        style_base(std::move(name))
    { (assign(props), ...); }

    name_type parent; //!< The style it inherits from; empty for none
    cell_props cell;
    paragraph_props para;
    text_props text;

    bool operator==(const cell_style &) const = default;

private:
    void assign(const cell_props &c) { cell = c; }
    void assign(const paragraph_props &p) { para = p; }
    void assign(const text_props &t) { text = t; }
};

/// A table row's style. The model holds no row properties yet, so it is kept
/// by name.
class row_style : public style_base<style_family::row>
{
public:
    explicit row_style(name_type name) :
        style_base(std::move(name))
    {
    }
    bool operator==(const row_style &) const = default;
};

/// A frame's style. The model holds no frame properties yet, so it is kept by
/// name.
class frame_style : public style_base<style_family::frame>
{
public:
    explicit frame_style(name_type name) :
        style_base(std::move(name))
    {
    }
    bool operator==(const frame_style &) const = default;
};

enum class list_enum : char
{
    arabic = '1',
    lower_alpha = 'a',
    upper_alpha = 'A',
    lower_roman = 'i',
    upper_roman = 'I',
};

/// A numbered level of a list style:
///
///     number_level{.level = 1, .format = list_enum::lower_alpha, .suffix = ")"}
struct number_level
{
    int level{1}; //!< Nesting level, from 1
    list_enum format{list_enum::arabic};
    std::string prefix;      //!< Before the number
    std::string suffix{"."}; //!< After the number
    int start_from{1};
    text_style_name style; //!< The number's text style
    text_props text{};     //!< The number's own text properties

    bool operator==(const number_level &) const = default;
};

/// Bullet characters for bullet_level.
struct bullet_type
{
    // Not constexpr: std::string only became a literal type in GCC 12's
    // libstdc++, and GCC 11 is supported.
    static std::string bullet() { return "•"; }
    static std::string black_circ() { return "●"; }
    static std::string heavy_check_mark() { return "✔"; }
    static std::string ballot_x() { return "✗"; }
    static std::string right_arrow() { return "➔"; }
    static std::string three_d_right_arrow() { return "➢"; }
};

/// A bulleted level of a list style:
///
///     bullet_level{.level = 2, .bullet = bullet_type::black_circ()}
struct bullet_level
{
    int level{1}; //!< Nesting level, from 1
    std::string bullet{bullet_type::bullet()};
    text_style_name style; //!< The bullet's text style

    bool operator==(const bullet_level &) const = default;
};

/// How a list numbers or bullets its items, level by level.
class list_style : public style_base<style_family::list>
{
public:
    using level = std::variant<number_level, bullet_level>;

    explicit list_style(name_type name, std::initializer_list<level> lvls = {}) :
        style_base(std::move(name)), levels(lvls)
    {
    }

    std::vector<level> levels;

    bool operator==(const list_style &) const = default;
};

/// The styles of one family, by name.
template <typename S>
using style_registry = named_registry<typename S::name_type, S>;

/// A style type: paragraph_style, text_style, ...
template <typename S>
concept style_type = std::is_base_of_v<style_base<S::family>, S>;

/// A document's styles, a registry per family:
///
///     doc.styles()[builtin::para].text.set(font_size(11));
///     doc.styles().para["Note"].para.set(space_after{pt(6)});
///     auto [strong, added] = doc.styles().insert(text_style("Strong",
///     text_props(font_weight::bold)));
///
/// A bare string names no family, so doc.styles()["Note"] doesn't compile:
/// pick the registry, as in styles().para["Note"].
class style_sheet
{
public:
    style_registry<paragraph_style> para;
    style_registry<text_style> text;
    style_registry<table_style> table;
    style_registry<cell_style> cell;
    style_registry<row_style> row;
    style_registry<frame_style> frame;
    style_registry<list_style> list;

    /// The registry of family F.
    template <style_family F>
    auto &of()
    { return of_impl<F>(*this); }
    template <style_family F>
    const auto &of() const
    { return of_impl<F>(*this); }

    /// The definition of that name in its family, added (empty) if there's
    /// none, as std::map::operator[]. The reference stays valid until the
    /// style is erased.
    template <style_family F>
    auto &operator[](const style_name<F> &name)
    { return of<F>()[name]; }

    /// The definition of that name in its family, or nullptr.
    template <style_family F>
    auto *find(const style_name<F> &name)
    { return of<F>().find(name); }
    template <style_family F>
    const auto *find(const style_name<F> &name) const
    { return of<F>().find(name); }

    /// Adds s to its family unless a style of its name is there, which is
    /// kept, as std::map::insert. Gives s's name, for set_style(), and whether
    /// s was added.
    template <style_type S>
    std::pair<typename S::name_type, bool> insert(S s)
    {
        auto [it, added] = of<S::family>().insert(std::move(s));
        return {it->first, added};
    }
    /// Adds s to its family, replacing any style of its name, as
    /// std::map::insert_or_assign. Gives s's name.
    template <style_type S>
    typename S::name_type insert_or_assign(S s)
    { return of<S::family>().insert_or_assign(std::move(s)).first->first; }

    /// Calls f with each family's registry.
    template <typename F>
    void for_each(F &&f)
    { f(para), f(text), f(table), f(cell), f(row), f(frame), f(list); }
    template <typename F>
    void for_each(F &&f) const
    { f(para), f(text), f(table), f(cell), f(row), f(frame), f(list); }

    bool empty() const
    {
        bool out = true;
        for_each([&](const auto &r) { out = out && r.empty(); });
        return out;
    }

    bool operator==(const style_sheet &) const = default;

private:
    template <style_family F, typename Self>
    static auto &of_impl(Self &self)
    {
        if constexpr(F == style_family::para)
            return self.para;
        else if constexpr(F == style_family::text)
            return self.text;
        else if constexpr(F == style_family::table)
            return self.table;
        else if constexpr(F == style_family::cell)
            return self.cell;
        else if constexpr(F == style_family::row)
            return self.row;
        else if constexpr(F == style_family::frame)
            return self.frame;
        else
            return self.list;
    }
};

/// CRTP mixin giving a node a style name of its family plus a fluent
/// set_style(). A derived type's defaulted operator== compares this base
/// subobject, so style equality composes automatically.
template <typename Derived, style_family F>
struct styled
{
    style_name<F> style;

    Derived &set_style(style_name<F> sn)
    {
        style = std::move(sn);
        return static_cast<Derived &>(*this);
    }

    bool operator==(const styled &) const = default;
};

} // namespace docsmith
