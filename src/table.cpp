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

#include <stdexcept>

#include "docsmithcpp/nodes.h"

namespace docsmith
{

table::table(size_t num_rows, size_t num_cols)
{
    rows.resize(num_rows);
    for(auto &row : rows)
        row.resize(num_cols);
}

table table::from_cells(std::vector<std::vector<table_cell>> cells)
{
    size_t cols = cells.empty() ? 0U : cells.front().size();

    // Require each row to have the same number of columns. Empty cells that are
    // spanned over are still placed.
    for(auto &row : cells)
        if(row.size() != cols)
            throw std::runtime_error("each row must have the same number of columns");

    table t;
    t.rows = std::move(cells);
    return t;
}

cell_view<table_cell> table::row(size_t row_idx)
{
    std::vector<table_cell *> cells;
    for(size_t col_idx{}; col_idx < num_cols(); col_idx += cell(row_idx, col_idx).colspan)
        cells.push_back(&rows.at(row_idx).at(col_idx));
    return cell_view<table_cell>(std::move(cells));
}
cell_view<table_cell> table::col(size_t col_idx)
{
    std::vector<table_cell *> cells;
    for(size_t row_idx{}; row_idx < num_rows(); row_idx += cell(row_idx, col_idx).rowspan)
        cells.push_back(&rows.at(row_idx).at(col_idx));
    return cell_view<table_cell>(std::move(cells));
}

cell_view<const table_cell> table::row(size_t row_idx) const
{
    std::vector<const table_cell *> cells;
    for(size_t col_idx{}; col_idx < num_cols(); col_idx += cell(row_idx, col_idx).colspan)
        cells.push_back(&rows.at(row_idx).at(col_idx));
    return cell_view<const table_cell>(std::move(cells));
}
cell_view<const table_cell> table::col(size_t col_idx) const
{
    std::vector<const table_cell *> cells;
    for(size_t row_idx{}; row_idx < num_rows(); row_idx += cell(row_idx, col_idx).rowspan)
        cells.push_back(&rows.at(row_idx).at(col_idx));
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

} // namespace docsmith
