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
#include <string>

#include <gtest/gtest.h>

#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/text_doc.h"

using namespace docsmith;

void print_compare(const text_doc &expected, const text_doc &actual)
{
    io_writer w(std::cout);
    std::cout << "===============\nExpected:\n===============\n";
    w(expected);
    std::cout << "===============\nActual:  \n===============\n";
    w(actual);
}

bool do_user_verification(std::string request)
{
    std::cout << request << "\n";
    std::cout << "Enter (pass|fail):";

    for(;;)
    {
        std::string result;
        std::cin >> result;
        if(result == "pass")
            return true;
        if(result == "fail")
            return false;
        std::cout << "Unexpected response: " << result << ". Try again\n";
    }
}

void open_file(std::string filename)
{
    std::string cmd = R"("start "" ")" + filename + R"(")";
    std::system(cmd.c_str());
}

/// Drop top-level pass-through nodes (e.g. text:sequence-decls that LibreOffice
/// emits at the start of office:text) so structural comparisons ignore them.
void drop_unknown(text_doc &doc)
{
    std::erase_if(doc.body, [](const block_node &b) { return std::holds_alternative<unknown>(b.v); });
}

/// Parse a text doc and strip all style names. Useful to compare a parsed
/// document with a code-generated one without minding style names.
text_doc parse_strip_styles(std::string filename)
{
    odt_file f(filename);
    text_doc doc = f.parse_text_doc();
    strip_styles(doc);
    drop_unknown(doc);
    return doc;
}

TEST(DOCSMITH, BasicUsage)
{
    heading h(1, "This is the heading");
    paragraph p("Some Paragraph", "some other span");

    auto h_copy = h;
    text_doc d{h_copy, p};

    EXPECT_EQ(d.body.size(), 2U);
}

TEST(DOCSMITH, UnknownPassThrough)
{
    // An unrecognized inline node survives a write -> parse -> write round trip.
    unknown raw{R"(<text:custom-thing foo="bar">hi</text:custom-thing>)"};

    text_doc doc;
    doc.add(paragraph().add(raw));

    odt_file f("odt/out/unknown_roundtrip.odt");
    f.save(doc);

    text_doc reparsed = f.parse_text_doc();
    auto unknowns = collect<unknown>(reparsed);
    ASSERT_FALSE(unknowns.empty());
    EXPECT_NE(unknowns.front()->xml.find("text:custom-thing"), std::string::npos);
}

TEST(ODT, ParseBasicFile)
{
    auto f = odt_file("odt/basic.odt");
    text_doc actual = f.parse_text_doc();
    strip_styles(actual);
    drop_unknown(actual);

    const text_doc expected{heading(1, "Heading 1"), paragraph("This is the first paragraph.")};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseModerateFile)
{
    text_doc actual = parse_strip_styles("odt/moderate.odt");

    // Styles are stripped from both sides, so compare content/structure only.
    text_doc expected{//
        heading(1, "Heading 1"),
        paragraph("First paragraph"),
        heading(2, "Second Heading"),
        paragraph("Second paragraph."),
        list(list_item(paragraph("First numbered list item, numbering starts at 7.")),
            list_item(paragraph("Item two."))),
        paragraph("Bullet points:"),
        list(list_item(paragraph("First; and")), list_item(paragraph("Second."))),
        paragraph("Alpha list:"),
        list(list_item(paragraph("Item a"))),
        list(list_item(list(list_item(paragraph("Sub items of alpha list are bullets"))))),
        list(list_item(paragraph("Item b"))),
        paragraph("Here is a url to ",
            hyperlink("https://github.com/MichaelCoutlakis/docsmithcpp", "docsmith"), " on ",
            span().add(span("GitHub")), ".")};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseImage)
{
    auto f = odt_file("odt/image.odt");
    text_doc actual = f.parse_text_doc();
    strip_styles(actual);
    drop_unknown(actual);

    text_doc expected{
        paragraph(frame(image{"Pictures/100000000000002A0000002A13903900F3783A21.png"}))};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseParagraphStyle)
{
    auto f = odt_file("odt/paragraph_with_style.odt");
    text_doc actual = f.parse_text_doc();
    drop_unknown(actual);
    const text_doc expected{paragraph("Paragraph with style.").set_style("Text_20_body")};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, WriteLists)
{
    style big_text("BigStyle", text_props(font_size(48)));

    list_style arabic("L0", {list_style_num(style{}, 1, list_enum::arabic, ".")});
    list_style arabic_from7("L1", {list_style_num(big_text, 1, list_enum::arabic, ")", 7)});
    list_style bullets("L2", {list_style_bullet(style{}, 1, bullet_type::bullet())});
    list_style alpha("L3", {list_style_num(style{}, 1, list_enum::lower_alpha, ")", 3)});

    text_doc d{
        list(list_item("First point"), list_item("Second point")).set_style(bullets),
        list(list_item("Number 1"), list_item(paragraph("Number 2").set_style(big_text)))
            .set_style(arabic),
        list(list_item("Number 7"), list_item("Number 8")).set_style(arabic_from7),
        list(list_item("Item a"), list_item("Item b")).set_style(alpha),
    };

    d.styles().add(big_text);
    d.list_styles().add(arabic);
    d.list_styles().add(arabic_from7);
    d.list_styles().add(bullets);
    d.list_styles().add(alpha);

    odt_file f("odt/out/write_lists.odt");
    f.save(d);

    io_writer w(std::cout);
    w(d);

    // open_file(f.filename());
}

TEST(ODT, GenerateBookmark)
{
    text_doc d;
    style style_page_break("PageBreak", paragraph_props(break_before{break_type::page}));

    d.add(paragraph("Some text"));
    d.add(paragraph("paragraph with bookmark", bookmark{"Bookmark 1"}));
    d.add(paragraph("Next paragraph with page break to ensure scrolling is necessary when bookmark "
                    "link is followed")
              .set_style(style_page_break));

    d.styles().add(style_page_break);

    d.add(paragraph(hyperlink("#Bookmark 1", "Link back to the bookmark")));

    odt_file f("odt/out/bookmark.odt");
    f.save(d);
    // open_file(f.filename());
}

TEST(ODT, ParseTable)
{
    text_doc actual = parse_strip_styles("odt/basic_table.odt");
    auto tables = collect<table>(actual);
    ASSERT_FALSE(tables.empty());
    table actual_table = *tables.front();

    table t(2, 2);
    t.row(0).assign(paragraph("Col 0"), paragraph("Col 1"));
    t.cell(1, 0).add(paragraph()); // LO empty table cell defaults to empty paragraph
    t.cell(1, 1).add(paragraph("Some text value"));

    print_compare(text_doc{t}, text_doc{actual_table});
    EXPECT_EQ(t, actual_table);

    text_doc expected{t};
    odt_file f("odt/out/table.odt");
    f.save(expected);
    // open_file(f.filename());
}

TEST(ODT, IntermediateTable)
{
    text_doc actual = parse_strip_styles("odt/table-intermediate.odt");
    auto tables = collect<table>(actual);
    ASSERT_FALSE(tables.empty());
    table ta = *tables.front();

    table tnest = table::from_cells({//
        {table_cell(paragraph("c(0,0)")), table_cell(paragraph("c(0,1)"))},
        {table_cell(paragraph("c(1,0)")), table_cell(paragraph("c(1,1)"))}});

    table te = table::from_cells({
        //
        {table_cell(paragraph("Col 0")), table_cell(paragraph("Col 1")),
            table_cell(paragraph("Col 2"))}, // header row
        {table_cell(paragraph("Merged Cells - row and col span")).set_span(2, 2), {},
            table_cell(paragraph("a"))},
        {{}, {}, table_cell(paragraph("b"))},
        {table_cell(paragraph("col span")).set_span(1, 2), {},
            table_cell(paragraph("row span")).set_span(2, 1)},
        {table_cell(paragraph("This cell contains a table and has no padding"), tnest),
            table_cell(paragraph("123")), {}} //
    });

    EXPECT_FALSE(te.is_cell_covered(2, 2));
    EXPECT_TRUE(te.is_cell_covered(1, 1));
    EXPECT_TRUE(te.is_cell_covered(2, 0));
    EXPECT_TRUE(te.is_cell_covered(2, 1));

    io_writer w(std::cout);
    w(text_doc{te});

    text_doc expected_doc{te};
    odt_file f("odt/out/table-intermediate.odt");
    f.save(expected_doc);
    // open_file(f.filename());
}
