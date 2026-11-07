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
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

#include "docsmithcpp/named_registry.h"

namespace docsmith
{

class style_name
{
public:
    style_name() = default;

    style_name(const char *name) :
        style_name(std::string(name))
    {
    }
    style_name(const std::string &name) :
        m_name{name}
    {
    }

    const std::string &get_name() const { return m_name; }
    bool is_empty() const { return m_name.empty(); }
    bool operator==(const style_name &rhs) const = default;

private:
    std::string m_name;
};

/// Style names writers know without a definition in text_doc::styles(). Each
/// writer maps them to its format's own style of that name.
///
/// The defaults are the styles of nodes with none of their own: an unstyled
/// paragraph has default_paragraph, and so on. Each is a definable style like
/// any other. Undefined, the format's own default applies; defined (in code, or
/// by the template loaded), it applies in every format. Reading a file gives
/// its defaults as these definitions and the nodes using them as unstyled.
namespace builtin
{
/// Paragraphs: LibreOffice's Default Paragraph Style, Word's Normal, and a
/// plain <p> in HTML. Other paragraph styles, headings' included, usually
/// derive from it, so a font set here applies to them too.
inline const style_name default_paragraph{"Standard"};
/// Headings of a level (1-10): LibreOffice's and Word's Heading 1, ..., and
/// <h1>, ... in HTML.
inline style_name heading(int level)
{ return style_name("Heading_20_" + std::to_string(std::clamp(level, 1, 10))); }
/// Links: LibreOffice's Internet Link, Word's Hyperlink, and a plain <a> in
/// HTML.
inline const style_name link{"Internet_20_link"};
/// Tables: Word's Normal Table. ODF has none of its own, so an ODT file names
/// it on its tables.
inline const style_name default_table{"Default_20_Table"};
/// Lists, as a list_style: a list with no style that isn't nested in another
/// (a nested one continues the enclosing list's style).
inline const style_name default_list{"Default_20_List"};
inline const style_name title{"Title"};
/// A monospace paragraph for code and logs, with its lines separated by '\n':
/// LibreOffice's Preformatted Text, and <pre> in HTML.
inline const style_name preformatted{"Preformatted_20_Text"};
/// Monospace text within a line: LibreOffice's Source Text, and <code> in HTML.
inline const style_name source_text{"Source_20_Text"};
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
    template <typename... Args>
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
    template <typename... Args>
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

/// The paragraph style for a cell's paragraphs that have no style of their
/// own (cell_props).
struct paragraph_style
{
    style_name m_name;
    bool operator==(const paragraph_style &rhs) const = default;
};

/// How a table cell looks: in a table_cell style, as a cell's direct
/// formatting (table_cell::props), or in a table style for its header rows
/// (header_cells). See DESIGN.md, Tables.
class cell_props
{
public:
    template <typename... Args>
        requires(!std::is_same_v<std::decay_t<Args>, cell_props> && ...)
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
    cell_props &set(const paragraph_style &p)
    {
        m_paragraph_style = p.m_name;
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
    style_name m_paragraph_style;
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
    if(!over.m_paragraph_style.is_empty())
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

/// How a table looks, in a table style. The lines and the header apply to
/// cells by their position when the document is written, under each cell's
/// own style and direct formatting. See DESIGN.md, Tables.
class table_props
{
public:
    template <typename... Args>
        requires(!std::is_same_v<std::decay_t<Args>, table_props> && ...)
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
};

/// What kind of node a style applies to. ODF and DOCX keep styles of different
/// families apart, so the same name may be defined once per family.
enum class style_family
{
    paragraph, //!< paragraph, heading
    text,      //!< span
    graphic,   //!< frame
    table,
    table_column,
    table_row,
    table_cell
};

///
class style
{
public:
    style() = default;
    template <typename... Args>
    explicit style(const style_name &name, const Args &...args) :
        m_name{name}
    {
        (set(args), ...);
    }

    style &set(style_family f)
    {
        m_family = f;
        return *this;
    }
    style &set(const text_props &props)
    {
        m_text_props = props;
        return *this;
    }
    style &set(const paragraph_props &props)
    {
        m_paragraph_props = props;
        return *this;
    }
    style &set(const table_props &props)
    {
        m_table_props = props;
        return *this;
    }
    style &set(const cell_props &props)
    {
        m_cell_props = props;
        return *this;
    }

    /// The style's properties of a kind, to change some and keep the rest
    /// (set() replaces them all). Added, empty, if it has none:
    ///
    ///     doc.builtin_style(builtin::heading(1)).text().set(font_size(20));
    text_props &text() { return m_text_props ? *m_text_props : m_text_props.emplace(); }
    paragraph_props &paragraph()
    { return m_paragraph_props ? *m_paragraph_props : m_paragraph_props.emplace(); }
    table_props &table() { return m_table_props ? *m_table_props : m_table_props.emplace(); }
    cell_props &cell() { return m_cell_props ? *m_cell_props : m_cell_props.emplace(); }

    operator style_name() const { return m_name; }
    bool operator==(const style &rhs) const = default;

    style_name m_name;
    /// Empty: writers that need a family work it out from the nodes that use
    /// the style, writing it once per family if they differ.
    std::optional<style_family> m_family;
    style_name m_parent_style;
    std::optional<text_props> m_text_props;
    std::optional<paragraph_props> m_paragraph_props;
    std::optional<table_props> m_table_props; //!< family table
    std::optional<cell_props> m_cell_props;   //!< family table_cell
};

inline std::string get_style_name(const style &s) { return s.m_name.get_name(); }

using style_registry = named_registry<style, get_style_name>;

/// CRTP mixin giving a node a style name plus a fluent set_style(). It is a
/// plain code-sharing base (no virtuals, no shared root), not the polymorphic
/// plumbing the old design used. A derived type's defaulted operator== compares
/// this base subobject, so style equality composes automatically.
template <typename Derived>
struct styled
{
    style_name style;

    Derived &set_style(style_name sn)
    {
        style = std::move(sn);
        return static_cast<Derived &>(*this);
    }

    bool operator==(const styled &) const = default;
};

} // namespace docsmith
