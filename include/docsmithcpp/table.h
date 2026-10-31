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

// Table nodes. table_cell holds block content (boxed); table is a grid of cells.

#include <cstddef>
#include <vector>

#include "docsmithcpp/nodes_fwd.h"

namespace docsmith
{

class table_cell : public styled<table_cell>, public has_children<block_node, table_cell, true>
{
public:
    int rowspan{1};
    int colspan{1};

    table_cell() = default;
    template <typename... Args, typename = content_ctor_t<table_cell, Args...>>
    table_cell(Args &&...content)
    { (this->add(std::forward<Args>(content)), ...); }

    table_cell &set_span(int rows, int cols)
    {
        rowspan = rows;
        colspan = cols;
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

    CellType &at(size_t idx) { return *m_cells.at(idx); }

private:
    std::vector<CellType *> m_cells;
};

class table
{
public:
    std::vector<std::vector<table_cell>> rows;

    table() = default;
    table(size_t num_rows, size_t num_cols);
    static table from_cells(std::vector<std::vector<table_cell>> cells);

    size_t num_cols() const { return rows.empty() ? 0U : rows.front().size(); }
    size_t num_rows() const { return rows.size(); }

    table_cell &cell(size_t r, size_t c) { return rows.at(r).at(c); }
    const table_cell &cell(size_t r, size_t c) const { return rows.at(r).at(c); }

    cell_view<table_cell> row(size_t row_idx);
    cell_view<table_cell> col(size_t col_idx);
    cell_view<const table_cell> row(size_t row_idx) const;
    cell_view<const table_cell> col(size_t col_idx) const;

    void add_row() { rows.push_back({}); }
    void add_cell(table_cell c) { rows.back().push_back(std::move(c)); }

    bool is_cell_covered(size_t row, size_t col) const;

    bool operator==(const table &) const = default;
};

} // namespace docsmith
