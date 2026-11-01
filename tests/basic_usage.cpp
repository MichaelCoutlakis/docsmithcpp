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

#include <iostream>

#include <gtest/gtest.h>

#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/text_doc.h"

using namespace docsmith;

TEST(BASIC_USAGE, CreateDocument)
{
    text_doc doc{
        paragraph(frame(image{"odt/smiley.png"}).set_style("fr1"), span("", "nested")),
    };

    io_writer w(std::cout);
    w(doc);

    odt::file(doc).save("odt/out/basic_generated.odt");
}

TEST(BASIC_USAGE, ValueSemantics)
{
    // Deep copy + structural equality come for free -- no clone(), no is_equal().
    text_doc doc{heading(1, "Title"), paragraph("body")};
    text_doc copy = doc;
    EXPECT_EQ(copy, doc);

    std::get<heading>(copy.body.front().v).level = 2;
    EXPECT_NE(copy, doc);
}

TEST(BASIC_USAGE, EqualityIncludesResources)
{
    text_doc a;
    a.add(paragraph(frame(a.add_image("Pictures/a.png", "bytes", "image/png"))));
    text_doc b = a;
    EXPECT_EQ(a, b);

    // Same name, same bytes, held separately: still equal.
    text_doc c;
    c.add(paragraph(frame(c.add_image("Pictures/a.png", "bytes", "image/png"))));
    EXPECT_EQ(a, c);

    c.add_image("Pictures/a.png", "other bytes", "image/png");
    EXPECT_EQ(a.body, c.body);
    EXPECT_NE(a, c);
}

TEST(BASIC_USAGE, ChainedAdd)
{
    // Children are added via .add() (chainable); nesting a same-type node uses
    // .add() too, so there is no copy-vs-child ambiguity.
    auto h = heading(2).set_style("H2").add("Title ").add(span("emphasis"));
    auto nested = span("outer ").add(span("inner"));

    EXPECT_EQ(h.level, 2);
    EXPECT_EQ(h.children().size(), 2U);
    EXPECT_EQ(nested.children().size(), 2U);
}

TEST(BASIC_USAGE, SetTitle)
{
    text_doc doc{paragraph("Body")};
    EXPECT_EQ(doc.find_title(), nullptr);

    doc.set_title("First");
    ASSERT_NE(doc.find_title(), nullptr);
    EXPECT_EQ(doc.body.front(), to_block(paragraph("First").set_style(builtin::title)));

    // A second call changes the title in place rather than adding another.
    doc.set_title("Second");
    EXPECT_EQ(doc.body.size(), 2U);
    EXPECT_EQ(*doc.find_title(), paragraph("Second").set_style(builtin::title));
    EXPECT_TRUE(doc.meta.title.empty());
}

TEST(BASIC_USAGE, FindTitleAnywhere)
{
    // A title placed by hand, after a logo, is still the title.
    text_doc doc{paragraph("Logo"), paragraph("Report").set_style(builtin::title)};
    doc.set_title("Renamed");
    EXPECT_EQ(doc.body.size(), 2U);
    EXPECT_EQ(doc.body.back(), to_block(paragraph("Renamed").set_style(builtin::title)));
}

TEST(BASIC_USAGE, AddStyle)
{
    text_doc doc;
    const style bold("Bold", text_props(font_weight::bold));
    EXPECT_EQ(doc.add_style(bold), style_name("Bold"));
    EXPECT_EQ(*doc.styles().find("Bold"), bold);

    EXPECT_NO_THROW(doc.add_style(bold)) << "an equal definition again is fine";
    EXPECT_THROW(
        doc.add_style(style("Bold", text_props(font_style::italic))), std::invalid_argument);
    EXPECT_EQ(*doc.styles().find("Bold"), bold);
}

TEST(BASIC_USAGE, AppendCombinesBodiesAndRegistries)
{
    text_doc doc{paragraph("first")};
    doc.add_style(style("Shared", text_props(font_weight::bold)));

    text_doc part{paragraph("second")};
    part.meta.title = "not copied";
    part.add_style(style("Shared", text_props(font_weight::bold)));
    part.add_style(style("Own", text_props(font_style::italic)));
    part.add(paragraph(frame(part.add_image("Pictures/a.png", "bytes", "image/png"))));

    doc.append(part);
    EXPECT_EQ(doc.body.size(), 3U);
    EXPECT_EQ(doc.body[1], to_block(paragraph("second")));
    EXPECT_EQ(doc.styles().size(), 2U);
    EXPECT_NE(doc.resources().find("Pictures/a.png"), nullptr);
    EXPECT_TRUE(doc.meta.title.empty());

    doc.append(doc);
    EXPECT_EQ(doc.body.size(), 6U);
}

TEST(BASIC_USAGE, AppendClashLeavesDocumentUnchanged)
{
    text_doc doc{paragraph("first")};
    doc.add_style(style("S", text_props(font_weight::bold)));
    const text_doc before = doc;

    text_doc part{paragraph("second")};
    part.add_style(style("Other", text_props(font_style::italic)));
    part.add_style(style("S", text_props(font_style::italic)));

    EXPECT_THROW(doc.append(part), std::invalid_argument);
    EXPECT_EQ(doc, before);
}

TEST(BASIC_USAGE, AddRowChecksWidth)
{
    table t;
    t.add_row({paragraph("a"), paragraph("b")});
    t.add_row({table_cell(paragraph("wide")).set_span(1, 2), {}});
    EXPECT_THROW(t.add_row({paragraph("c")}), std::invalid_argument);
    EXPECT_TRUE(t.is_rectangular());

    t.add_row();
    t.add_cell(paragraph("d"));
    EXPECT_FALSE(t.is_rectangular());
}

TEST(BASIC_USAGE, ReplacePlaceholders)
{
    text_doc doc{heading(1, "Report"), paragraph(" {{summary}} "),
        paragraph(bookmark{"details"}, "Details go here"),
        paragraph("Placeholder style").set_style("Placeholder")};

    EXPECT_TRUE(doc.replace(text{"{{summary}}"}, {heading(2, "Summary"), table(1, 1)}));
    EXPECT_TRUE(doc.replace(bookmark{"details"}, paragraph("details")));
    EXPECT_TRUE(doc.replace(style_name("Placeholder"), text_doc{}));
    EXPECT_FALSE(doc.replace(text{"{{summary}}"}, paragraph("again"))) << "already replaced";

    const text_doc expected{heading(1, "Report"), heading(2, "Summary"), table(1, 1),
        paragraph("details")};
    EXPECT_EQ(doc.body, expected.body);
}

TEST(BASIC_USAGE, InsertAroundPlaceholder)
{
    text_doc doc{paragraph("a"), paragraph("{{x}}"), paragraph("b")};
    EXPECT_TRUE(doc.insert_after(text{"{{x}}"}, {paragraph("after 1"), paragraph("after 2")}));
    EXPECT_TRUE(doc.insert_before(text{"{{x}}"}, paragraph("before")));
    const text_doc expected{paragraph("a"), paragraph("before"), paragraph("{{x}}"),
        paragraph("after 1"), paragraph("after 2"), paragraph("b")};
    EXPECT_EQ(doc.body, expected.body);
}

TEST(BASIC_USAGE, PlaceholderInCellsAndHeader)
{
    table t(1, 1);
    t.cell(0, 0).add(paragraph("{{cell}}"));
    text_doc doc{t};
    doc.page.set_footer(paragraph("{{footer}}"));

    EXPECT_TRUE(doc.replace(text{"{{cell}}"}, paragraph("in a cell")));
    EXPECT_TRUE(doc.replace(text{"{{footer}}"}, paragraph("in the footer")));
    EXPECT_EQ(collect<table>(doc).front()->cell(0, 0).children().front(),
        to_block(paragraph("in a cell")));
    EXPECT_EQ(doc.page.footer.front(), to_block(paragraph("in the footer")));
}

TEST(BASIC_USAGE, FillMergesRegistries)
{
    text_doc doc{paragraph("{{x}}")};
    style bold("Bold", text_props(font_weight::bold));

    text_doc part;
    part.add(paragraph("bold").set_style(part.add_style(bold)));
    EXPECT_TRUE(doc.replace(text{"{{x}}"}, part));
    EXPECT_NE(doc.styles().find("Bold"), nullptr);

    // A clash throws and leaves the document as it was.
    text_doc clash{paragraph("y")};
    clash.add_style(style("Bold", text_props(font_style::italic)));
    const text_doc before = doc;
    EXPECT_THROW(doc.insert_after(text{"bold"}, clash), std::invalid_argument);
    EXPECT_EQ(doc, before);
}
