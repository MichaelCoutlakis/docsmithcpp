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

#include <gtest/gtest.h>

#include "docsmithcpp/text_doc.h"

using namespace docsmith;

namespace
{
const border thin = border::solid(pt(0.5));
const border thick = border::solid(pt(1.5), colour::rgb(0x20, 0x40, 0x60));
} // namespace

TEST(Table, AddRowChecksWidth)
{
    table t;
    t.add_row({paragraph("a"), paragraph("b")});
    t.add_row({table_cell(paragraph("wide")).set_span(1, 2), {}}, "Row style");
    EXPECT_THROW(t.add_row({paragraph("c")}), std::invalid_argument);
    EXPECT_EQ(t.num_rows(), 2U);
    EXPECT_EQ(t.num_cols(), 2U);
    EXPECT_EQ(t.row_style(1), row_style_name("Row style"));
    EXPECT_EQ(t.cells().size(), 4U);
    EXPECT_THROW(table::from_cells({{"a", "b"}, {"c"}}), std::invalid_argument);
}

TEST(Table, RemoveRowKeepsHeaderCount)
{
    table t = table::from_cells({{"h1"}, {"h2"}, {"body"}});
    t.set_header_rows(2);
    t.remove_row(0);
    EXPECT_EQ(t.header_rows(), 1U);
    EXPECT_EQ(t.cell(0, 0), table_cell("h2"));
    t.remove_row(1);
    EXPECT_EQ(t.header_rows(), 1U);
    EXPECT_EQ(t.num_rows(), 1U);
}

TEST(Table, BuildThenFormat)
{
    table t = table::from_cells({{"Test", "Result"}, {"open", "pass"}});
    t.add_row({"save", "fail"});
    t.set_header_rows(1).set_style("Results").set_column_widths({3, 1});
    t.set_row_borders({0}, edge::bottom, thick);

    // The line below the header is on both sides of it.
    for(size_t c = 0; c != 2; ++c)
    {
        EXPECT_EQ(t.cell(0, c).props.m_borders.m_bottom, thick);
        EXPECT_EQ(t.cell(1, c).props.m_borders.m_top, thick);
        EXPECT_FALSE(t.cell(2, c).props.m_borders.m_top);
    }
    EXPECT_EQ(column_shares(t), (std::vector<double>{0.75, 0.25}));
}

TEST(Table, ColumnSharesFillGaps)
{
    table t(1, 3);
    EXPECT_EQ(column_shares(t), (std::vector<double>{1.0 / 3, 1.0 / 3, 1.0 / 3}));
    t.set_column_width(0, 1).set_column_width(1, 3);
    EXPECT_EQ(column_shares(t), (std::vector<double>{1.0 / 6, 3.0 / 6, 2.0 / 6}));
}

TEST(Table, BordersSkipSpannedCells)
{
    // A B C
    // A A C   (A spans two rows and two columns)
    // D E F
    table t(3, 3);
    t.cell(0, 0).set_span(2, 2);
    t.set_horiz_borders(lines::inner, thin);

    // Line 1 runs through A, so only C gets it.
    EXPECT_FALSE(t.cell(0, 0).props.m_borders.m_top);
    EXPECT_EQ(t.cell(0, 0).props.m_borders.m_bottom, thin); // line 2
    EXPECT_EQ(t.cell(0, 2).props.m_borders.m_bottom, thin);
    EXPECT_EQ(t.cell(1, 2).props.m_borders.m_top, thin);
    EXPECT_TRUE(t.cell(1, 0).props.empty()) << "placeholders are left alone";
    EXPECT_EQ(t.cell(2, 1).props.m_borders.m_top, thin);
    EXPECT_FALSE(t.cell(2, 1).props.m_borders.m_bottom) << "the outer line isn't inner";

    t.set_vert_borders(lines::outer, thick);
    EXPECT_EQ(t.cell(0, 0).props.m_borders.m_start, thick);
    EXPECT_FALSE(t.cell(0, 0).props.m_borders.m_end);
    EXPECT_EQ(t.cell(1, 2).props.m_borders.m_end, thick);
    EXPECT_EQ(t.origin(1, 1), (std::pair<size_t, size_t>{0, 0}));
}

TEST(Table, SetBordersDrawsAGrid)
{
    table t(2, 2);
    t.set_borders(thin);
    for(const auto &c : t.cells())
        EXPECT_EQ(c.props.m_borders, (cell_borders{thin, thin, thin, thin}));
}

TEST(Table, EffectivePropsLayers)
{
    text_doc doc;
    doc.styles().insert(table_style("Grid",
        table_props(outer_border{thick},
            inside_h{thin},
            inside_v{thin},
            cell_padding{pt(2)},
            header_cells{cell_props(colour::rgb(0xdd, 0xdd, 0xdd),
                cell_borders{.m_bottom = thick},
                para_style_name("Table Heading"))})));
    doc.styles().insert(cell_style("Base cell", cell_props(cell_valign::middle)));
    cell_style highlight("Highlight", cell_props(colour::rgb(0xff, 0xff, 0)));
    highlight.parent = "Base cell";
    doc.styles().insert(highlight);

    table t = table::from_cells({{"h", "h"}, {"a", "b"}, {"c", "d"}});
    t.set_header_rows(1).set_style("Grid");
    t.cell(1, 1).set_style("Highlight");
    t.cell(2, 1).props.m_borders.m_top = border::none();

    const auto header = effective_cell_props(t, doc.styles(), 0, 0);
    EXPECT_EQ(header.m_borders, (cell_borders{thick, thick, thick, thin}));
    EXPECT_EQ(header.m_background, colour::rgb(0xdd, 0xdd, 0xdd));
    EXPECT_EQ(header.m_padding, pt(2));

    const auto body = effective_cell_props(t, doc.styles(), 1, 1);
    EXPECT_EQ(body.m_borders, (cell_borders{thick, thin, thin, thick}));
    EXPECT_EQ(body.m_background, colour::rgb(0xff, 0xff, 0)) << "the cell's style";
    EXPECT_EQ(body.m_valign, cell_valign::middle) << "its parent's";

    const auto direct = effective_cell_props(t, doc.styles(), 2, 1);
    EXPECT_EQ(direct.m_borders.m_top, border::none()) << "direct beats the table style";
    EXPECT_EQ(direct.m_borders.m_bottom, thick);

    // Without the table style's lines (DOCX), the header's still apply.
    const auto no_lines = effective_cell_props(t, doc.styles(), 0, 0, false);
    EXPECT_EQ(no_lines.m_borders, (cell_borders{{}, thick, {}, {}}));
    EXPECT_FALSE(no_lines.m_padding);

    // Live: a change to the style shows on the next look.
    doc.styles().table.find("Grid")->table.m_inside_v = thick;
    EXPECT_EQ(effective_cell_props(t, doc.styles(), 1, 0).m_borders.m_end, thick);
}

TEST(Table, ColumnStyleIsLive)
{
    text_doc doc;
    doc.styles().insert(table_style("Grid",
        table_props(header_cells{
            cell_props(colour::rgb(0xdd, 0xdd, 0xdd), para_style_name("Table Heading"))})));
    doc.styles().insert(cell_style("Key",
        cell_props(
            colour::rgb(0xee, 0xee, 0xee), cell_valign::middle, para_style_name("Key text"))));
    doc.styles().insert(cell_style("Highlight", cell_props(colour::rgb(0xff, 0xff, 0))));

    table t = table::from_cells({{"h", "h"}, {"a", "b"}});
    t.set_header_rows(1).set_style("Grid").set_column_style(0, "Key");
    t.add_row({"c", "d"}); // after the column's style was set
    t.cell(1, 0).set_style("Highlight");

    const auto added = effective_cell_props(t, doc.styles(), 2, 0);
    EXPECT_EQ(added.m_background, colour::rgb(0xee, 0xee, 0xee)) << "a row added later";
    EXPECT_EQ(added.m_paragraph_style, para_style_name("Key text"));

    const auto header = effective_cell_props(t, doc.styles(), 0, 0);
    EXPECT_EQ(header.m_background, colour::rgb(0xdd, 0xdd, 0xdd)) << "the header beats the column";
    EXPECT_EQ(header.m_paragraph_style, para_style_name("Table Heading"));
    EXPECT_EQ(header.m_valign, cell_valign::middle) << "the column's, where the header has none";

    const auto own = effective_cell_props(t, doc.styles(), 1, 0);
    EXPECT_EQ(own.m_background, colour::rgb(0xff, 0xff, 0)) << "the cell's own style beats both";
    EXPECT_EQ(own.m_paragraph_style, para_style_name("Key text"));

    EXPECT_EQ(effective_cell_props(t, doc.styles(), 1, 1), cell_props{});
}

TEST(Table, CellTextFromColumnAndCellStyles)
{
    text_doc doc;
    cell_style key("Key", text_props(font_weight::bold), paragraph_props(text_align::centre));
    doc.styles().insert(key);
    cell_style small("Small", text_props(font_size(8)));
    small.parent = "Key";
    doc.styles().insert(small);

    table t = table::from_cells({{"a", "b"}});
    t.set_column_style(0, "Key");
    t.cell(0, 1).set_style("Small");

    EXPECT_EQ(effective_cell_text(t, doc.styles(), 0, 0),
        (cell_text{paragraph_props(text_align::centre), text_props(font_weight::bold)}));
    EXPECT_EQ(effective_cell_text(t, doc.styles(), 0, 1),
        (cell_text{
            paragraph_props(text_align::centre), text_props(font_weight::bold, font_size(8))}))
        << "the parent's under its own";
}

TEST(Table, PropsCopy)
{
    cell_props a(colour::rgb(1, 2, 3));
    cell_props b(a);
    table_props c(percent{50});
    table_props d(c);
    EXPECT_EQ(a, b);
    EXPECT_EQ(c, d);
}

TEST(Table, DefaultTableStyle)
{
    text_doc doc;
    doc.styles().insert(
        table_style(builtin::table, table_props(outer_border{thin}, cell_padding{pt(2)})));
    const table t = table::from_cells({{"a"}});
    EXPECT_EQ(effective_table_props(t, doc.styles()).m_padding, pt(2));
    EXPECT_EQ(effective_cell_props(t, doc.styles(), 0, 0).m_borders.m_top, thin)
        << "an unstyled table has the default's lines";
}

TEST(Table, ViewsSkipCoveredSlots)
{
    // A A B
    // C D B   (A spans two columns, B two rows)
    table t(2, 3);
    t.cell(0, 0).set_span(1, 2);
    t.cell(0, 2).set_span(2, 1);

    EXPECT_EQ(t.row(0).size(), 2u);
    t.row(1).assign("C", "D");
    EXPECT_EQ(t.cell(1, 0), table_cell("C"));
    EXPECT_EQ(t.cell(1, 1), table_cell("D"));
    EXPECT_EQ(t.col(1).size(), 1u) << "(0, 1) is covered by A";
    EXPECT_EQ(std::as_const(t).col(2).size(), 1u);
    EXPECT_TRUE(t.is_cell_covered(0, 1));
    EXPECT_TRUE(t.is_cell_covered(1, 2));
    EXPECT_FALSE(t.is_cell_covered(1, 1));
}

TEST(Table, SpanMustBePositive)
{
    EXPECT_THROW(table_cell().set_span(1, 0), std::invalid_argument);
    EXPECT_THROW(table_cell().set_span(-1, 1), std::invalid_argument);

    // A bad span set directly doesn't hang the views.
    table t(1, 2);
    t.cell(0, 0).colspan = 0;
    EXPECT_EQ(t.row(0).size(), 2u);
    EXPECT_FALSE(t.is_cell_covered(0, 1));
}
