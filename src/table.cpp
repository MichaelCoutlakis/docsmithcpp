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

#include "docsmithcpp/table.h"

namespace docsmith
{

table::table(size_t rows, size_t cols)
{
    m_cells.resize(rows);
    for(auto &row : m_cells)
        row.resize(cols);
}

table table::from_cells(std::vector<std::vector<table_cell>> cells)
{
    // Determine num rows / col
    size_t rows = cells.size();
    size_t cols = cells.empty() ? 0U : cells.front().size();

    // Require each row to have the same number of columns: Empty cells that are spanned are still
    // placed:
    for(auto &row : cells)
        if(row.size() != cols)
            throw std::runtime_error("each row must have the same number of columns");

    table t;
    t.m_cells = cells;
    return t;
}

cell_view<table_cell> table::row(size_t row_idx)
{
    std::vector<table_cell *> cells;
    for(size_t col_idx{}; col_idx < num_cols(); col_idx += cell(row_idx, col_idx).colspan())
        cells.push_back(&m_cells.at(row_idx).at(col_idx));

    return cell_view<table_cell>(cells);
}
cell_view<table_cell> table::col(size_t col_idx)
{
    std::vector<table_cell *> cells;
    for(size_t row_idx{}; row_idx < num_rows(); row_idx += cell(row_idx, col_idx).rowspan())
        cells.push_back(&m_cells.at(row_idx).at(col_idx));
    return cell_view<table_cell>(cells);
}

cell_view<const table_cell> table::row(size_t row_idx) const
{
    std::vector<const table_cell *> cells;
    for(size_t col_idx{}; col_idx < num_cols(); col_idx += cell(row_idx, col_idx).colspan())
        cells.push_back(&m_cells.at(row_idx).at(col_idx));

    return cell_view<const table_cell>(cells);
}
cell_view<const table_cell> table::col(size_t col_idx) const
{
    std::vector<const table_cell *> cells;
    for(size_t row_idx{}; row_idx < num_rows(); row_idx += cell(row_idx, col_idx).rowspan())
        cells.push_back(&m_cells.at(row_idx).at(col_idx));
    return cell_view<const table_cell>(std::move(cells));
}

std::list<element *> table::children()
{
    std::list<element *> kids;
    for(auto &r : m_cells)
        for(auto &c : r)
            kids.push_back(&c);
    return kids;
}

bool table::is_cell_covered(size_t row, size_t col) const
{
    if(row >= num_rows() || col >= num_cols())
        throw std::out_of_range("table cell out of range");

    // any of the preceeding cells (row or column wise) could cover the cell in question
    for(size_t r = 0; r <= row; ++r)
    {
        for(size_t c = 0; c <= col; ++c)
        {
            if(r + cell(r, c).rowspan() > row &&  // covered in the row dimension
                c + cell(r, c).colspan() > col && // covered in the col dimension
                !(r == row && c == col))          // don't compare cell to itself
                return true;
        }
    }
    return false;
}

}