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
#include <string>
#include <vector>

#include "docsmithcpp/forward_decl.h"
#include "docsmithcpp/nodes.h"
#include "docsmithcpp/style.h"

namespace docsmith
{

class table_cell : public element_base<table_cell>,
                   public nodes<table_cell>,
                   public styled<table_cell>,
                   public elem_tagged<table_cell, elem_t::cell>
{
public:
    // Re-use the nodes constructor:
    using nodes<table_cell>::nodes;

    bool operator==(const table_cell &rhs) const
    {
        return m_rowspan == rhs.m_rowspan && m_colspan == rhs.m_colspan && children_equal(rhs);
    }
    size_t rowspan() const { return m_rowspan; }
    size_t colspan() const { return m_colspan; }

    size_t &rowspan() { return m_rowspan; }
    size_t &colspan() { return m_colspan; }

private:
    size_t m_rowspan{1};
    size_t m_colspan{1};
};

/// Provides a view of cells, for example, for a specific row or column.
template <typename CellType>
class cell_view
{
public:
    using value_type = CellType;

    class cell_view(std::vector<CellType *> cells) :
        m_cells(cells)
    {
    }

    template <typename... Args>
    cell_view &assign(Args &&...args)
    {
        size_t idx = 0;
        auto assign_cell = [this, &idx](auto &&value)
        {
            auto &cell = *m_cells.at(idx++);
            cell.children().clear();
            cell.add(value);
        };
        (assign_cell(std::forward<Args>(args)), ...);
        return *this;
    }

    CellType &at(size_t idx) { return *m_cells.at(idx); }

private:
    std::vector<CellType *> m_cells;
};

class table : public element_base<table>,
              public styled<table>,
              public elem_tagged<table, elem_t::tbl>
{
public:
    table() { }
    table(size_t rows, size_t cols);

    // caption?

    size_t num_cols() const { return m_cells.empty() ? 0 : m_cells.front().size(); }
    size_t num_rows() const { return m_cells.size(); }

    table_cell &cell(size_t row, size_t col) { return m_cells.at(row).at(col); }
    const table_cell &cell(size_t row, size_t col) const { return m_cells.at(row).at(col); }

    cell_view<table_cell> row(size_t row_idx);
    cell_view<table_cell> col(size_t col_idx);

    cell_view<const table_cell> row(size_t row_idx) const;
    cell_view<const table_cell> col(size_t col_idx) const;

    bool operator==(const table &rhs) const { return m_cells == rhs.m_cells; }

    /// Add an empty row to the end of the table
    void add_row() { m_cells.push_back({}); }

    /// Add a cell to the last row of the table
    void add_cell(table_cell &&cell) { m_cells.back().push_back(cell); }

    // table needs to implement children() as it doesn't store cells directly as nodes:
    std::list<element *> children() override;

private:
    std::vector<std::vector<table_cell>> m_cells;
};

}