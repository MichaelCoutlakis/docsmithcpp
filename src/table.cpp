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
#include <iterator>
#include <stdexcept>

#include "docsmithcpp/nodes.h"

namespace docsmith
{

table::table(size_t num_rows, size_t num_cols) :
    m_cells(num_rows * num_cols), m_cols(num_cols), m_rows(num_rows), m_columns(num_cols)
{
}

table table::from_cells(std::vector<std::vector<table_cell>> cells)
{
    table t;
    for(auto &row : cells)
        t.add_row(std::move(row));
    return t;
}

table_cell &table::cell(size_t r, size_t c)
{
    if(r >= num_rows() || c >= num_cols())
        throw std::out_of_range("table cell out of range");
    return m_cells[r * m_cols + c];
}

const table_cell &table::cell(size_t r, size_t c) const
{
    if(r >= num_rows() || c >= num_cols())
        throw std::out_of_range("table cell out of range");
    return m_cells[r * m_cols + c];
}

table &table::add_row(std::vector<table_cell> cells, style_name row_style)
{
    if(m_rows.empty() && m_cols == 0)
    {
        m_cols = cells.size();
        m_columns.resize(m_cols);
    }
    else if(cells.size() != m_cols)
        throw std::invalid_argument("each row must have the same number of columns");
    std::move(cells.begin(), cells.end(), std::back_inserter(m_cells));
    m_rows.push_back({std::move(row_style)});
    return *this;
}

void table::remove_row(size_t r)
{
    if(r >= num_rows())
        throw std::out_of_range("table row out of range");
    const auto first = m_cells.begin() + static_cast<std::ptrdiff_t>(r * m_cols);
    m_cells.erase(first, first + static_cast<std::ptrdiff_t>(m_cols));
    m_rows.erase(m_rows.begin() + static_cast<std::ptrdiff_t>(r));
    if(r < m_header_rows)
        --m_header_rows;
}

table &table::set_row_style(size_t r, style_name sn)
{
    m_rows.at(r).style = std::move(sn);
    return *this;
}

table &table::set_column_widths(const std::vector<double> &widths)
{
    if(widths.size() != m_cols)
        throw std::invalid_argument("one width per column");
    for(size_t c = 0; c != m_cols; ++c)
        m_columns[c].width = widths[c];
    return *this;
}

table &table::set_column_width(size_t c, std::optional<double> width)
{
    m_columns.at(c).width = width;
    return *this;
}

table &table::set_column_style(size_t c, style_name sn)
{
    m_columns.at(c).style = std::move(sn);
    return *this;
}

cell_view<table_cell> table::row(size_t row_idx)
{
    std::vector<table_cell *> cells;
    for(size_t col_idx{}; col_idx < num_cols(); col_idx += cell(row_idx, col_idx).colspan)
        cells.push_back(&cell(row_idx, col_idx));
    return cell_view<table_cell>(std::move(cells));
}
cell_view<table_cell> table::col(size_t col_idx)
{
    std::vector<table_cell *> cells;
    for(size_t row_idx{}; row_idx < num_rows(); row_idx += cell(row_idx, col_idx).rowspan)
        cells.push_back(&cell(row_idx, col_idx));
    return cell_view<table_cell>(std::move(cells));
}

cell_view<const table_cell> table::row(size_t row_idx) const
{
    std::vector<const table_cell *> cells;
    for(size_t col_idx{}; col_idx < num_cols(); col_idx += cell(row_idx, col_idx).colspan)
        cells.push_back(&cell(row_idx, col_idx));
    return cell_view<const table_cell>(std::move(cells));
}
cell_view<const table_cell> table::col(size_t col_idx) const
{
    std::vector<const table_cell *> cells;
    for(size_t row_idx{}; row_idx < num_rows(); row_idx += cell(row_idx, col_idx).rowspan)
        cells.push_back(&cell(row_idx, col_idx));
    return cell_view<const table_cell>(std::move(cells));
}

bool table::is_cell_covered(size_t row, size_t col) const
{
    if(row >= num_rows() || col >= num_cols())
        throw std::out_of_range("table cell out of range");

    // Any of the preceding cells (row- or column-wise) could cover this cell.
    for(size_t r = 0; r <= row; ++r)
    {
        for(size_t c = 0; c <= col; ++c)
        {
            if(r + static_cast<size_t>(cell(r, c).rowspan) > row && // covered row-wise
                c + static_cast<size_t>(cell(r, c).colspan) > col && // covered col-wise
                !(r == row && c == col))                             // not the cell itself
                return true;
        }
    }
    return false;
}

std::vector<size_t> table::origins() const
{
    std::vector<size_t> out(m_cells.size());
    for(size_t i = 0; i != out.size(); ++i)
        out[i] = i;
    for(size_t r = 0; r != num_rows(); ++r)
        for(size_t c = 0; c != m_cols; ++c)
        {
            const size_t i = r * m_cols + c;
            if(out[i] != i)
                continue; // covered
            const auto &cl = m_cells[i];
            for(size_t dr = 0;
                dr < static_cast<size_t>(std::max(cl.rowspan, 1)) && r + dr < num_rows();
                ++dr)
                for(size_t dc = 0;
                    dc < static_cast<size_t>(std::max(cl.colspan, 1)) && c + dc < m_cols;
                    ++dc)
                    if(const size_t j = (r + dr) * m_cols + c + dc; j != i && out[j] == j)
                        out[j] = i;
        }
    return out;
}

std::pair<size_t, size_t> table::origin(size_t row, size_t col) const
{
    if(row >= num_rows() || col >= num_cols())
        throw std::out_of_range("table cell out of range");
    const size_t i = origins()[row * m_cols + col];
    return {i / m_cols, i % m_cols};
}

namespace
{
/// The rows (or columns) a cell spans, kept inside the table.
size_t extent(int span, size_t at, size_t size)
{ return std::min(static_cast<size_t>(std::max(span, 1)), size - at); }

std::vector<size_t> line_indices(lines which, size_t count)
{
    std::vector<size_t> out;
    if(count == 0)
        return out;
    for(size_t i = 0; i <= count; ++i)
        if(which == lines::all || (which == lines::inner) == (i != 0 && i != count))
            out.push_back(i);
    return out;
}
} // namespace

void table::set_horiz_line(const std::vector<size_t> &origins, size_t i, const border &b)
{
    for(size_t c = 0; c != m_cols; ++c)
    {
        if(i > 0 && i <= num_rows())
        {
            auto &above = m_cells[origins[(i - 1) * m_cols + c]];
            const size_t r = origins[(i - 1) * m_cols + c] / m_cols;
            if(r + extent(above.rowspan, r, num_rows()) == i)
                above.props.m_borders.m_bottom = b;
        }
        if(i < num_rows())
        {
            auto &below = m_cells[origins[i * m_cols + c]];
            if(origins[i * m_cols + c] / m_cols == i)
                below.props.m_borders.m_top = b;
        }
    }
}

void table::set_vert_line(const std::vector<size_t> &origins, size_t i, const border &b)
{
    for(size_t r = 0; r != num_rows(); ++r)
    {
        if(i > 0 && i <= m_cols)
        {
            auto &before = m_cells[origins[r * m_cols + i - 1]];
            const size_t c = origins[r * m_cols + i - 1] % m_cols;
            if(c + extent(before.colspan, c, m_cols) == i)
                before.props.m_borders.m_end = b;
        }
        if(i < m_cols)
        {
            auto &after = m_cells[origins[r * m_cols + i]];
            if(origins[r * m_cols + i] % m_cols == i)
                after.props.m_borders.m_start = b;
        }
    }
}

table &table::set_borders(const border &b)
{
    set_horiz_borders(lines::all, b);
    return set_vert_borders(lines::all, b);
}

table &table::set_outer_border(const border &b)
{
    set_horiz_borders(lines::outer, b);
    return set_vert_borders(lines::outer, b);
}

table &table::set_horiz_borders(lines which, const border &b)
{ return set_horiz_borders(line_indices(which, num_rows()), b); }

table &table::set_horiz_borders(const std::vector<size_t> &which, const border &b)
{
    const auto o = origins();
    for(size_t i : which)
        set_horiz_line(o, i, b);
    return *this;
}

table &table::set_vert_borders(lines which, const border &b)
{ return set_vert_borders(line_indices(which, num_cols()), b); }

table &table::set_vert_borders(const std::vector<size_t> &which, const border &b)
{
    const auto o = origins();
    for(size_t i : which)
        set_vert_line(o, i, b);
    return *this;
}

table &table::set_row_borders(const std::vector<size_t> &rows, edge edges, const border &b)
{
    std::vector<size_t> which;
    for(size_t r : rows)
    {
        if(has(edges, edge::top))
            which.push_back(r);
        if(has(edges, edge::bottom))
            which.push_back(r + 1);
    }
    return set_horiz_borders(which, b);
}

table &table::set_column_borders(const std::vector<size_t> &cols, edge edges, const border &b)
{
    std::vector<size_t> which;
    for(size_t c : cols)
    {
        if(has(edges, edge::start))
            which.push_back(c);
        if(has(edges, edge::end))
            which.push_back(c + 1);
    }
    return set_vert_borders(which, b);
}

std::vector<double> column_shares(const table &t)
{
    double total = 0;
    size_t given = 0;
    for(size_t c = 0; c != t.num_cols(); ++c)
        if(const auto w = t.column_width(c); w && *w > 0)
        {
            total += *w;
            ++given;
        }
    const double fallback = given ? total / static_cast<double>(given) : 1;
    std::vector<double> out;
    double sum = 0;
    for(size_t c = 0; c != t.num_cols(); ++c)
    {
        const auto w = t.column_width(c);
        out.push_back(w && *w > 0 ? *w : fallback);
        sum += out.back();
    }
    for(auto &s : out)
        s /= sum;
    return out;
}

namespace
{
/// sn and its parents, nearest first.
std::vector<const style *> lineage(const style_name &sn, const style_registry &styles)
{
    std::vector<const style *> out;
    const style *s = styles.find(sn.get_name());
    for(int depth = 0; s && depth != 32; ++depth) // guards against a cycle
    {
        out.push_back(s);
        s = styles.find(s->m_parent_style.get_name());
    }
    return out;
}

template <typename T>
void take(std::optional<T> &into, const std::optional<T> &from)
{
    if(from)
        into = from;
}

void overlay(cell_borders &into, const cell_borders &over)
{
    take(into.m_top, over.m_top);
    take(into.m_bottom, over.m_bottom);
    take(into.m_start, over.m_start);
    take(into.m_end, over.m_end);
}
} // namespace

cell_props effective_cell_style(const style_name &sn, const style_registry &styles)
{
    cell_props out;
    const auto chain = lineage(sn, styles);
    for(auto it = chain.rbegin(); it != chain.rend(); ++it)
        if((*it)->m_cell_props)
            out.overlay(*(*it)->m_cell_props);
    return out;
}

table_props effective_table_props(const table &t, const style_registry &styles)
{ return effective_table_style(t.style.is_empty() ? builtin::default_table : t.style, styles); }

table_props effective_table_style(const style_name &sn, const style_registry &styles)
{
    table_props out;
    const auto chain = lineage(sn, styles);
    for(auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        if(!(*it)->m_table_props)
            continue;
        const table_props &p = *(*it)->m_table_props;
        take(out.m_width, p.m_width);
        take(out.m_align, p.m_align);
        overlay(out.m_outer, p.m_outer);
        take(out.m_inside_h, p.m_inside_h);
        take(out.m_inside_v, p.m_inside_v);
        take(out.m_padding, p.m_padding);
        out.m_header.overlay(p.m_header);
    }
    return out;
}

cell_props effective_cell_props(
    const table &t, const style_registry &styles, size_t row, size_t col, bool table_lines)
{
    const table_cell &c = t.cell(row, col);
    const size_t rows = t.num_rows(), cols = t.num_cols();
    const size_t last_row = row + extent(c.rowspan, row, rows);
    const size_t last_col = col + extent(c.colspan, col, cols);
    const size_t header = std::min(t.header_rows(), rows);

    const table_props tp = effective_table_props(t, styles);
    cell_props out;
    auto &b = out.m_borders;
    if(table_lines)
    {
        b.m_top = row == 0 ? tp.m_outer.m_top : tp.m_inside_h;
        b.m_bottom = last_row == rows ? tp.m_outer.m_bottom : tp.m_inside_h;
        b.m_start = col == 0 ? tp.m_outer.m_start : tp.m_inside_v;
        b.m_end = last_col == cols ? tp.m_outer.m_end : tp.m_inside_v;
        out.m_padding = tp.m_padding;
    }
    out.overlay(effective_cell_style(t.column_style(col), styles));
    if(row < header)
        out.overlay(tp.m_header);
    // The line below the header is also the top of the row after it, so the
    // two agree.
    else if(row == header && row != 0 && tp.m_header.m_borders.m_bottom)
        b.m_top = tp.m_header.m_borders.m_bottom;
    out.overlay(effective_cell_style(c.style, styles));
    out.overlay(c.props);
    return out;
}

} // namespace docsmith
