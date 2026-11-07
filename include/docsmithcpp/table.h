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

// Table nodes. table_cell holds block content (boxed); table is a grid of cells
// with per-row and per-column formatting.

#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "docsmithcpp/nodes_fwd.h"

namespace docsmith
{

class table_cell : public styled<table_cell>, public has_children<block_node, table_cell, true>
{
public:
    int rowspan{1};
    int colspan{1};
    /// Direct formatting, over the cell's style and the table's. See DESIGN.md,
    /// Tables.
    cell_props props;

    table_cell() = default;
    template <typename... Args, typename = content_ctor_t<table_cell, Args...>>
    table_cell(Args &&...content)
    { (this->add(std::forward<Args>(content)), ...); }

    /// Throws std::invalid_argument unless rows and cols are at least 1.
    table_cell &set_span(int rows, int cols)
    {
        if(rows < 1 || cols < 1)
            throw std::invalid_argument("a cell spans at least one row and column");
        rowspan = rows;
        colspan = cols;
        return *this;
    }
    table_cell &set_props(cell_props p)
    {
        props = std::move(p);
        return *this;
    }

    bool operator==(const table_cell &) const = default;
};

/// A view over a selection of cells (e.g. a row or a column).
template <typename CellType>
class cell_view
{
public:
    using value_type = CellType;

    explicit cell_view(std::vector<CellType *> cells) :
        m_cells(std::move(cells))
    {
    }

    /// Replace the contents of each viewed cell, in order, with one block each.
    template <typename... Args>
    cell_view &assign(Args &&...args)
    {
        size_t idx = 0;
        auto assign_cell = [this, &idx](auto &&value)
        {
            auto &c = *m_cells.at(idx++);
            c.children().clear();
            c.children().push_back(to_block(std::forward<decltype(value)>(value)));
        };
        (assign_cell(std::forward<Args>(args)), ...);
        return *this;
    }

    cell_view &set_style(const style_name &sn)
    {
        for(auto *c : m_cells)
            c->set_style(sn);
        return *this;
    }
    cell_view &set_background(const colour &c)
    {
        for(auto *cell : m_cells)
            cell->props.set(c);
        return *this;
    }

    CellType &at(size_t idx) { return *m_cells.at(idx); }
    size_t size() const { return m_cells.size(); }

private:
    std::vector<CellType *> m_cells;
};

/// Edges of a cell, row or column, combined with |.
enum class edge : unsigned
{
    top = 1,
    bottom = 2,
    start = 4,
    end = 8
};
constexpr edge operator|(edge a, edge b)
{ return static_cast<edge>(static_cast<unsigned>(a) | static_cast<unsigned>(b)); }
constexpr bool has(edge set, edge e)
{ return (static_cast<unsigned>(set) & static_cast<unsigned>(e)) != 0; }

/// Which of a table's horizontal or vertical lines. A table with n rows has
/// n + 1 horizontal lines, line i being above row i; columns likewise.
enum class lines
{
    all,
    inner, //!< between rows (or columns)
    outer  //!< the first and last
};

/// A grid of cells, always rectangular: every row has num_cols() cells. A slot
/// spanned over by an earlier cell still holds a (placeholder) cell; writers
/// skip it (is_cell_covered()).
class table : public styled<table>
{
public:
    /// The table's name in formats that name tables (ODT); empty lets the
    /// writer choose.
    std::string name;
    /// Alternative text for screen readers and accessible PDF, not shown: a
    /// short title and a longer description. Not a visible caption, which is
    /// a paragraph of its own.
    std::string accessibility_title;
    std::string accessibility_description;

    table() = default;
    table(size_t num_rows, size_t num_cols);
    /// Throws std::invalid_argument unless every row has the same number of
    /// cells.
    static table from_cells(std::vector<std::vector<table_cell>> cells);

    size_t num_cols() const { return m_cols; }
    size_t num_rows() const { return m_rows.size(); }

    /// The cell in a slot. A slot covered by another cell's span holds a
    /// placeholder that writers skip, so content put in it is not written:
    /// origin() gives the cell that covers it.
    table_cell &cell(size_t r, size_t c);
    const table_cell &cell(size_t r, size_t c) const;
    /// Every cell, row by row, placeholders included.
    std::span<table_cell> cells() { return m_cells; }
    std::span<const table_cell> cells() const { return m_cells; }

    /// The cells that start in a row or column, in order: slots covered by a
    /// span from another cell are left out.
    cell_view<table_cell> row(size_t row_idx);
    cell_view<table_cell> col(size_t col_idx);
    cell_view<const table_cell> row(size_t row_idx) const;
    cell_view<const table_cell> col(size_t col_idx) const;

    /// Appends a row. The first row of a table with no columns sets
    /// num_cols(); after that, throws std::invalid_argument unless cells has
    /// num_cols() cells.
    table &add_row(std::vector<table_cell> cells, style_name row_style = {});
    void remove_row(size_t r);

    /// The first header_rows() rows are header rows: they repeat on each page
    /// in ODT and DOCX, and are <thead> / <th> in HTML.
    size_t header_rows() const { return m_header_rows; }
    table &set_header_rows(size_t n)
    {
        m_header_rows = n;
        return *this;
    }

    const style_name &row_style(size_t r) const { return m_rows.at(r).style; }
    table &set_row_style(size_t r, style_name sn);

    /// A column's width relative to the others' (e.g. 1 and 3 for a quarter
    /// and three quarters), or nullopt for an equal share.
    std::optional<double> column_width(size_t c) const { return m_columns.at(c).width; }
    /// One width per column, as column_width(). Throws std::invalid_argument
    /// unless widths has num_cols() values.
    table &set_column_widths(const std::vector<double> &widths);
    table &set_column_width(size_t c, std::optional<double> width);
    /// A cell style (style_family::table_cell) for the column's cells, under
    /// the header rows' formatting and each cell's own style. It applies by
    /// position when the document is written, so rows added later get it too.
    const style_name &column_style(size_t c) const { return m_columns.at(c).style; }
    table &set_column_style(size_t c, style_name cell_style);

    // Borders as the cells' direct formatting (table_cell::props). They act on
    // the cells there are now; a line between two cells is set on both.

    /// Every line, inside and out.
    table &set_borders(const border &b);
    table &set_outer_border(const border &b);
    table &set_horiz_borders(lines which, const border &b);
    /// The horizontal lines with these indices (line i is above row i).
    table &set_horiz_borders(const std::vector<size_t> &which, const border &b);
    table &set_vert_borders(lines which, const border &b);
    table &set_vert_borders(const std::vector<size_t> &which, const border &b);
    /// The top and/or bottom of these rows.
    table &set_row_borders(const std::vector<size_t> &rows, edge edges, const border &b);
    /// The start and/or end of these columns.
    table &set_column_borders(const std::vector<size_t> &cols, edge edges, const border &b);

    bool is_cell_covered(size_t row, size_t col) const;
    /// The row and column of the cell that covers a slot: the slot itself if
    /// it isn't covered.
    std::pair<size_t, size_t> origin(size_t row, size_t col) const;

    bool operator==(const table &) const = default;

private:
    struct row_format
    {
        style_name style;
        bool operator==(const row_format &) const = default;
    };
    struct column_format
    {
        std::optional<double> width;
        style_name style;
        bool operator==(const column_format &) const = default;
    };

    /// For each slot, the index of the cell that covers it (itself if none).
    std::vector<size_t> origins() const;
    void set_horiz_line(const std::vector<size_t> &origins, size_t i, const border &b);
    void set_vert_line(const std::vector<size_t> &origins, size_t i, const border &b);

    std::vector<table_cell> m_cells; //!< Row by row
    size_t m_cols{};
    std::vector<row_format> m_rows;
    std::vector<column_format> m_columns;
    size_t m_header_rows{};
};

/// How the cell at (row, col) looks once these are combined, the later
/// winning for each property: its table's style's lines, its column's style,
/// the table style's header_cells (in a header row), its own style and its
/// direct formatting. Each style has its parents' properties filled in. With table_lines false, the table
/// style's outer and inside lines and padding are left out, for formats that
/// write those in the table style itself (DOCX).
cell_props effective_cell_props(
    const table &t, const style_registry &styles, size_t row, size_t col, bool table_lines = true);

/// Each column's share of the table's width, summing to 1: column_width()
/// over their total, a column without one taking the others' average.
std::vector<double> column_shares(const table &t);

/// The table's style's table_props, with its parents' filled in: those of
/// builtin::default_table for a table with no style.
table_props effective_table_props(const table &t, const style_registry &styles);

/// A style's table_props, with its parents' filled in.
table_props effective_table_style(const style_name &sn, const style_registry &styles);

/// A style's cell_props, with its parents' filled in.
cell_props effective_cell_style(const style_name &sn, const style_registry &styles);

} // namespace docsmith
