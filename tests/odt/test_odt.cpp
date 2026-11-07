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

/// Drop top-level pass-through nodes (e.g. text:sequence-decls that LibreOffice
/// emits at the start of office:text) so structural comparisons ignore them.
void drop_unknown(text_doc &doc)
{
    std::erase_if(doc.body, [](const block_node &b) { return std::holds_alternative<unknown>(b.v); });
}

/// Load a document and strip all style names. Useful to compare a parsed
/// document with a code-generated one without minding style names.
text_doc load_strip_styles(const std::string &filename)
{
    text_doc doc = odt::file::load(filename).doc;
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
    // An unrecognized inline node survives a write -> parse round trip.
    unknown raw{R"(<text:custom-thing foo="bar">hi</text:custom-thing>)"};

    text_doc doc;
    doc.add(paragraph().add(raw));
    odt::file(doc).save("odt/out/unknown_roundtrip.odt");

    text_doc reparsed = odt::file::load("odt/out/unknown_roundtrip.odt").doc;
    auto unknowns = collect<unknown>(reparsed);
    ASSERT_FALSE(unknowns.empty());
    EXPECT_NE(unknowns.front()->xml.find("text:custom-thing"), std::string::npos);
}

TEST(ODT, ParseBasicFile)
{
    text_doc actual = odt::file::load("odt/basic.odt").doc;
    strip_styles(actual);
    drop_unknown(actual);

    const text_doc expected{heading(1, "Heading 1"), paragraph("This is the first paragraph.")};

    print_compare(expected, actual);
    EXPECT_EQ(expected.body, actual.body);
}

TEST(ODT, ParseModerateFile)
{
    text_doc actual = load_strip_styles("odt/moderate.odt");

    // "Item b" carries on the count of "Item a" (text:continue-list).
    auto continued = [](list l)
    {
        l.continue_numbering = true;
        return l;
    };
    // Styles are stripped, so compare content and structure only.
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
        continued(list(list_item(paragraph("Item b")))),
        paragraph("Here is a url to ",
            hyperlink("https://github.com/MichaelCoutlakis/docsmithcpp", "docsmith"), " on ",
            span().add(span("GitHub")), ".")};

    print_compare(expected, actual);
    EXPECT_EQ(expected.body, actual.body);
}

TEST(ODT, ParseImage)
{
    text_doc actual = odt::file::load("odt/image.odt").doc;
    strip_styles(actual);
    drop_unknown(actual);

    frame f(image{"Pictures/100000000000002A0000002A13903900F3783A21.png"});
    f.width = f.height = pt(31.493); // 1.111cm
    text_doc expected{paragraph(f)};

    print_compare(expected, actual);
    EXPECT_EQ(expected.body, actual.body);

    // The image's bytes move from the package into the document.
    const resource *r =
        actual.resources().find("Pictures/100000000000002A0000002A13903900F3783A21.png");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->media_type(), "image/png");
    EXPECT_EQ(r->data().substr(1, 3), "PNG");
}

TEST(ODT, ParseLists)
{
    // As LibreOffice saved it: a list header, an item numbered from 5, a list
    // carrying on the count after a paragraph, and one starting again whose
    // unnumbered entry LibreOffice saved as more of the item before it.
    text_doc actual = load_strip_styles("odt/lists.odt");

    list first(list_item("Header"), list_item("One"), list_item("Five"), list_item("Six"));
    first.items()[0].numbered = false;
    first.items()[2].start_value = 5;
    list second(list_item("Seven"));
    second.continue_numbering = true;
    const text_doc expected{first,
        paragraph("Between"),
        second,
        list(list_item("Restart", "Mid header"), list_item("Two"))};

    print_compare(expected, actual);
    EXPECT_EQ(expected.body, actual.body);
}

TEST(ODT, ParseParagraphStyle)
{
    text_doc actual = odt::file::load("odt/paragraph_with_style.odt").doc;
    drop_unknown(actual);
    const text_doc expected{paragraph("Paragraph with style.").set_style("Text body")};

    print_compare(expected, actual);
    EXPECT_EQ(expected.body, actual.body);
}

TEST(ODT, ResaveLoadedFile)
{
    // load -> save -> load gives back the same document, including style names.
    for(const char *name :
        {"basic", "moderate", "paragraph_with_style", "table-intermediate", "lists"})
    {
        const auto first = odt::file::load(std::string("odt/") + name + ".odt");
        const std::string resaved = std::string("odt/out/") + name + "_resaved.odt";
        first.save(resaved);
        EXPECT_EQ(first.doc, odt::file::load(resaved).doc) << name;
    }
}

TEST(ODT, ResaveLoadedImage)
{
    // The image lives in the document's resources (Pictures/...), not on disk,
    // so saving must embed it from there.
    const auto first = odt::file::load("odt/image.odt");
    ASSERT_NO_THROW(first.save("odt/out/image_resaved.odt"));

    const auto again = odt::file::load("odt/out/image_resaved.odt");
    EXPECT_EQ(first.doc, again.doc); // resources included

    // ...and the saved file can itself be saved again.
    EXPECT_NO_THROW(again.save("odt/out/image_resaved2.odt"));
}

TEST(ODT, AddedImageRoundTrips)
{
    text_doc doc;
    doc.add(paragraph(frame(doc.add_image("Pictures/dot.png", "not really a png", "image/png"))));
    odt::file(doc).save("odt/out/added_image.odt");

    EXPECT_EQ(odt::file::load("odt/out/added_image.odt").doc.resources(), doc.resources());
}

TEST(ODT, ColumnStyleRoundTrips)
{
    text_doc doc;
    doc.styles().insert(paragraph_style("Key text", text_props(font_weight::bold)));
    doc.styles().insert(cell_style(
        "Key", cell_props(background(colour::rgb(0xee, 0xee, 0xee)), para_style_name("Key text"))));
    table t = table::from_cells({{"a", "1"}, {"b", "2"}});
    t.set_column_style(0, "Key");
    doc.add(t);
    odt::file(doc).save("odt/out/column_style.odt");

    const auto again = odt::file::load("odt/out/column_style.odt").doc;
    const table &back = *collect<table>(again).front();
    EXPECT_EQ(back.column_style(0), cell_style_name("Key"));
    EXPECT_TRUE(back.column_style(1).empty());
    EXPECT_EQ(effective_cell_props(back, again.styles(), 1, 0).m_background,
        colour::rgb(0xee, 0xee, 0xee));
}

// LibreOffice ignores a cell style's paragraph and text properties, so they
// go on the cell's unstyled paragraphs, as a paragraph style made from them.
TEST(ODT, CellStyleTextGoesOnItsParagraphs)
{
    text_doc doc;
    doc.styles().insert(
        cell_style("Key", text_props(font_weight::bold), paragraph_props(text_align::centre)));
    table t = table::from_cells({{"a", "1"}, {paragraph("b").set_style("Own"), {"2"}}});
    t.set_column_style(0, "Key");
    doc.add(t);
    odt::file(doc).save("odt/out/cell_text.odt");

    const auto again = odt::file::load("odt/out/cell_text.odt");
    const table &back = *collect<table>(again.doc).front();
    const auto &a = std::get<paragraph>(back.cell(0, 0).items().front().v);
    EXPECT_EQ(a.style, para_style_name("Key Text"));
    const paragraph_style *made = again.doc.styles().find(a.style);
    ASSERT_NE(made, nullptr);
    EXPECT_EQ(made->text, text_props(font_weight::bold));
    EXPECT_EQ(made->para, paragraph_props(text_align::centre));
    EXPECT_EQ(made->parent, builtin::para);
    EXPECT_EQ(std::get<paragraph>(back.cell(1, 0).items().front().v).style, para_style_name("Own"))
        << "a styled paragraph keeps its style";
    EXPECT_TRUE(std::get<paragraph>(back.cell(0, 1).items().front().v).style.empty());
    EXPECT_EQ(again.doc.styles().cell.find("Key")->text, text_props(font_weight::bold))
        << "kept in the cell style too";

    // Saved again, the made style is the same one.
    again.save("odt/out/cell_text_resaved.odt");
    EXPECT_EQ(odt::file::load("odt/out/cell_text_resaved.odt").doc.styles(), again.doc.styles());
}

TEST(ODT, StyledLinkRoundTrips)
{
    text_doc doc;
    doc.styles().insert(text_style("Quiet link", text_props(font_colour(colour::rgb(80, 80, 80)))));
    doc.add(paragraph("see ", hyperlink("https://example.com", "site").set_style("Quiet link")));
    odt::file(doc).save("odt/out/styled_link.odt");
    EXPECT_EQ(odt::file::load("odt/out/styled_link.odt").doc.body, doc.body);
}

TEST(ODT, ParagraphPropsRoundTrip)
{
    text_doc doc;
    const paragraph_props pp(space_before{pt(12)},
        space_after{pt(6)},
        indent_start{pt(18)},
        indent_end{pt(9)},
        first_line_indent{pt(-9)},
        text_align::centre,
        keep_with_next{},
        keep_together{false});
    doc.styles().insert(paragraph_style("Spaced", pp));
    doc.add(paragraph("x").set_style("Spaced"));
    odt::file(doc).save("odt/out/paragraph_props.odt");
    const auto again = odt::file::load("odt/out/paragraph_props.odt").doc;
    ASSERT_NE(again.styles().para.find("Spaced"), nullptr);
    EXPECT_EQ(again.styles().para.find("Spaced")->para, pp);
}

TEST(ODT, BlockFrameRoundTrips)
{
    text_doc doc;
    const image img = doc.add_image("Pictures/dot.png", "not really a png", "image/png");
    doc.add(frame(img));
    doc.add(paragraph("Text then ", frame(img)));
    odt::file(doc).save("odt/out/block_frame.odt");

    const auto again = odt::file::load("odt/out/block_frame.odt").doc;
    ASSERT_EQ(again.body.size(), 2U);
    EXPECT_EQ(again.body[0], doc.body[0]) << "an unstyled paragraph of its own";
    EXPECT_EQ(again.body[1], doc.body[1]);
}

TEST(ODT, EditLoadedFile)
{
    auto f = odt::file::load("odt/basic.odt");
    f.doc.add(paragraph("Appended by the test."));
    f.save("odt/out/basic_edited.odt");

    const auto edited = odt::file::load("odt/out/basic_edited.odt");
    ASSERT_EQ(edited.doc.body.size(), f.doc.body.size());
    EXPECT_EQ(edited.doc.body.back(), to_block(paragraph("Appended by the test.")));
}

TEST(ODT, MissingImageThrows)
{
    text_doc doc{paragraph(frame(image{"no/such/image.png"}))};
    EXPECT_THROW(odt::file(doc).save("odt/out/missing_image.odt"), docsmith::error);
}

TEST(ODT, WriteLists)
{
    text_style big_text("BigStyle", text_props(font_size(48)));

    list_style arabic("L0", {number_level{.level = 1, .format = list_enum::arabic, .suffix = "."}});
    list_style arabic_from7("L1",
        {number_level{.level = 1,
            .format = list_enum::arabic,
            .suffix = ")",
            .start_from = 7,
            .style = big_text.name()}});
    list_style bullets("L2", {bullet_level{.level = 1, .bullet = bullet_type::bullet()}});
    list_style alpha("L3",
        {number_level{
            .level = 1, .format = list_enum::lower_alpha, .suffix = ")", .start_from = 3}});

    text_doc d{
        list(list_item("First point"), list_item("Second point")).set_style(bullets.name()),
        list(list_item("Number 1"),
            list_item(paragraph("Number 2", span("big").set_style(big_text.name()))))
            .set_style(arabic.name()),
        list(list_item("Number 7"), list_item("Number 8")).set_style(arabic_from7.name()),
        list(list_item("Item a"), list_item("Item b")).set_style(alpha.name()),
    };

    d.styles().insert_or_assign(big_text);
    d.styles().insert(arabic);
    d.styles().insert(arabic_from7);
    d.styles().insert(bullets);
    d.styles().insert(alpha);

    odt::file(d).save("odt/out/write_lists.odt");

    io_writer w(std::cout);
    w(d);
}

TEST(ODT, GenerateBookmark)
{
    text_doc d;
    paragraph_style style_page_break("PageBreak", paragraph_props(break_before{break_type::page}));

    d.add(paragraph("Some text"));
    d.add(paragraph("paragraph with bookmark", bookmark{"Bookmark 1"}));
    d.add(paragraph("Next paragraph with page break to ensure scrolling is necessary when bookmark "
                    "link is followed")
            .set_style(style_page_break.name()));

    d.styles().insert_or_assign(style_page_break);

    d.add(paragraph(hyperlink(bookmark{"Bookmark 1"}, "Link back to the bookmark")));

    odt::file(d).save("odt/out/bookmark.odt");
}

TEST(ODT, ReadTableFormatting)
{
    // basic_table.odt: a 77% wide table, centred, with LibreOffice's borders
    // on its cells.
    const text_doc doc = odt::file::load("odt/basic_table.odt").doc;
    const table &t = *collect<table>(doc).front();
    EXPECT_EQ(t.style, table_style_name("Table1"));
    const table_props tp = effective_table_props(t, doc.styles());
    EXPECT_EQ(tp.m_width, (std::variant<length, percent>(percent{77})));
    EXPECT_EQ(tp.m_align, table_align::centre);

    const border line = border::solid(pt(0.5));
    const cell_props a1 = effective_cell_props(t, doc.styles(), 0, 0);
    EXPECT_EQ(a1.m_borders, (cell_borders{line, line, line, border::none()}));
    EXPECT_NEAR(a1.m_padding->m_points, 0.097 * 72 / 2.54, 0.0005); // to the thousandth of a point
    EXPECT_EQ(effective_cell_props(t, doc.styles(), 0, 1).m_borders,
        (cell_borders{line, line, line, line}));
}

TEST(ODT, TableFormattingRoundTrip)
{
    text_doc doc;
    const border thick = border::solid(pt(1.5), colour::rgb(0x20, 0x40, 0x60));
    const border thin = border::solid(pt(0.5));
    doc.styles().insert(table_style("Grid",
        table_props(table_width(percent{80}),
            table_align::centre,
            outer_border{thick},
            inside_h{thin},
            header_cells{cell_props(
                background(colour::rgb(0xdd, 0xdd, 0xdd)), cell_borders{.m_bottom = thick})})));
    table t = table::from_cells({{"Test", "Result"}, {"open", "pass"}, {"save", "fail"}});
    t.set_header_rows(1).set_style("Grid").set_column_widths({3, 1});
    t.cell(2, 1).props.set(background(colour::rgb(0xff, 0xcc, 0xcc)));
    doc.add(t);
    odt::file(doc).save("odt/out/table_formatting.odt");

    // The table style's lines come back on the cells. A line between two rows
    // is the upper cell's bottom only, as LibreOffice draws both.
    const text_doc again = odt::file::load("odt/out/table_formatting.odt").doc;
    const table &back = *collect<table>(again).front();
    EXPECT_NEAR(column_shares(back).at(0), 0.75, 1e-4);
    for(size_t r = 0; r != t.num_rows(); ++r)
        for(size_t c = 0; c != t.num_cols(); ++c)
        {
            auto expected = effective_cell_props(t, doc.styles(), r, c);
            if(r != 0)
                expected.m_borders.m_top = border::none();
            EXPECT_EQ(effective_cell_props(back, again.styles(), r, c), expected) << r << ", " << c;
        }
    EXPECT_EQ(effective_table_props(back, again.styles()).m_width,
        (std::variant<length, percent>(percent{80})));
}

TEST(ODT, ParseTable)
{
    text_doc actual = load_strip_styles("odt/basic_table.odt");
    auto tables = collect<table>(actual);
    ASSERT_FALSE(tables.empty());
    table actual_table = *tables.front();

    table t = table(2, 2).set_header_rows(1);
    t.row(0).assign(paragraph("Col 0"), paragraph("Col 1"));
    t.cell(1, 0).add(paragraph()); // LO empty table cell defaults to empty paragraph
    t.cell(1, 1).add(paragraph("Some text value"));
    t.name = "Table1";
    t.set_column_widths({3759, 3759}); // LibreOffice's relative widths

    print_compare(text_doc{t}, text_doc{actual_table});
    EXPECT_EQ(t, actual_table);

    odt::file(text_doc{t}).save("odt/out/table.odt");
}

TEST(ODT, IntermediateTable)
{
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

    odt::file(text_doc{te}).save("odt/out/table-intermediate.odt");
}

TEST(ODT, ReadMetadata)
{
    using namespace std::chrono;
    const auto meta = odt::file::load("odt/basic.odt").doc.meta;
    EXPECT_EQ(meta.created, sys_days{2025y / June / 16} + 13h + 50min + 22s);
    EXPECT_EQ(meta.modified, sys_days{2025y / June / 16} + 21h + 23min + 36s);
    EXPECT_TRUE(meta.title.empty());
}

TEST(ODT, MetadataRoundTrips)
{
    using namespace std::chrono;
    text_doc doc{paragraph("Body")};
    doc.meta = {.title = "The Title",
        .author = "A. Writer",
        .language = "en-ZA",
        .subject = "Testing",
        .description = "A <short> & sweet summary",
        .keywords = {"one", "two"},
        .created = sys_days{2025y / October / 31} + 9h,
        .custom = {{"Reviewed", "yes"}}};
    odt::file(doc).save("odt/out/metadata.odt");

    EXPECT_EQ(odt::file::load("odt/out/metadata.odt").doc.meta, doc.meta);
}

TEST(ODT, EditMetadataKeepsTheRest)
{
    auto f = odt::file::load("odt/basic.odt");
    f.doc.meta.title = "Retitled";
    f.doc.meta.keywords = {"edited"};
    f.save("odt/out/basic_retitled.odt");

    const auto again = odt::file::load("odt/out/basic_retitled.odt");
    EXPECT_EQ(again.doc.meta, f.doc.meta);

    // meta.xml keeps what the model doesn't hold, and the original dates as written.
    again.save("odt/out/basic_retitled2.odt");
    EXPECT_EQ(odt::file::load("odt/out/basic_retitled2.odt").doc.meta, f.doc.meta);
}

TEST(ODT, NewDocumentWithoutMetadata)
{
    odt::file(text_doc{paragraph("Body")}).save("odt/out/no_metadata.odt");
    EXPECT_EQ(odt::file::load("odt/out/no_metadata.odt").doc.meta, metadata{});
}

TEST(ODT, TitleStyleIsDefined)
{
    text_doc doc;
    doc.set_title("The Title");
    odt::file(doc).save("odt/out/title.odt");

    const auto again = odt::file::load("odt/out/title.odt");
    ASSERT_NE(again.doc.find_title(), nullptr);
    EXPECT_EQ(*again.doc.find_title(), *doc.find_title());

    // Saved once, defined once.
    again.save("odt/out/title_resaved.odt");
    EXPECT_EQ(odt::file::load("odt/out/title_resaved.odt").doc, again.doc);
}

TEST(ODT, LinkTargetsRoundTrip)
{
    text_doc doc{paragraph("target", bookmark{"Bookmark 1"}),
        paragraph(hyperlink(bookmark{"Bookmark 1"}, "back"), hyperlink("https://example.com", "out"))};
    odt::file(doc).save("odt/out/link_targets.odt");

    const auto loaded = odt::file::load("odt/out/link_targets.odt");
    const auto links = collect<hyperlink>(loaded.doc);
    ASSERT_EQ(links.size(), 2U);
    EXPECT_EQ(links[0]->target, link_target(bookmark_target{"Bookmark 1"}));
    EXPECT_EQ(links[1]->target, link_target(url_target{"https://example.com"}));
}

TEST(ODT, StyleFamilies)
{
    text_doc doc;
    auto red = doc.styles()
                   .insert(text_style("Red", text_props(font_colour(colour::rgb(255, 0, 0)))))
                   .first;
    auto note = doc.styles().insert(paragraph_style("Note", text_props(font_style::italic))).first;
    // The same name in two families: two styles.
    auto both = doc.styles().insert(text_style("Both", text_props(font_weight::bold))).first;
    auto both_para =
        doc.styles().insert(paragraph_style("Both", paragraph_props(text_align::centre))).first;
    doc.add(paragraph("Some ", span("red").set_style(red), " text.").set_style(note));
    doc.add(paragraph("Bold ", span("span").set_style(both)).set_style(both_para));
    odt::file(doc).save("odt/out/style_families.odt");

    const auto again = odt::file::load("odt/out/style_families.odt").doc;
    EXPECT_EQ(again.body, doc.body);
    EXPECT_NE(again.styles().text.find("Red"), nullptr);
    EXPECT_EQ(*again.styles().para.find("Note"), *doc.styles().para.find("Note"));
    EXPECT_EQ(*again.styles().text.find("Both"), *doc.styles().text.find("Both"));
    EXPECT_EQ(*again.styles().para.find("Both"), *doc.styles().para.find("Both"));
}

TEST(ODT, LoadStyleDefinitions)
{
    const text_doc doc = odt::file::load("odt/moderate.odt").doc;

    // Common styles from styles.xml.
    const paragraph_style *h1 = doc.styles().find(builtin::heading(1));
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->parent, para_style_name("Heading"));
    EXPECT_EQ(h1->text.m_font_size, font_size(18));
    EXPECT_EQ(h1->text.m_font_weight, font_weight::bold);
    EXPECT_NE(doc.styles().find(builtin::source_text), nullptr);
    EXPECT_EQ(doc.styles().find(builtin::link)->text.m_font_colour, colour::rgb(0, 0, 0x80));

    // Automatic styles from content.xml.
    EXPECT_EQ(doc.styles().para.find("P3")->parent, builtin::heading(1));
    EXPECT_EQ(doc.styles().text.find("T1")->text.m_font_weight, font_weight::normal);

    const list_style *l1 = doc.styles().list.find("L1");
    ASSERT_NE(l1, nullptr);
    const auto &first = std::get<number_level>(l1->levels.front());
    EXPECT_EQ(first.level, 1);
    EXPECT_EQ(first.format, list_enum::arabic);
    EXPECT_EQ(first.start_from, 7);
    EXPECT_EQ(first.suffix, ".");
    EXPECT_EQ(first.style, text_style_name("Numbering Symbols"));
    EXPECT_TRUE(std::holds_alternative<bullet_level>(doc.styles().list.find("L2")->levels.front()));
    EXPECT_EQ(std::get<number_level>(doc.styles().list.find("L3")->levels.front()).format,
        list_enum::lower_alpha);
}

TEST(ODT, CodeBuiltStylesRoundTrip)
{
    text_doc doc;
    auto big =
        doc.styles()
            .insert(paragraph_style("Big",
                text_props(font_size(20.5f),
                    font_name("Liberation Mono"),
                    font_style::italic,
                    font_colour(colour::rgb(1, 2, 3))),
                paragraph_props(break_before{break_type::page}, break_after{break_type::column})))
            .first;
    auto pic = doc.styles().insert(frame_style("Pic")).first;
    auto steps = doc.styles()
                     .insert(list_style("Steps",
                         {number_level{.level = 1,
                              .format = list_enum::upper_roman,
                              .prefix = "(",
                              .suffix = ")",
                              .start_from = 2},
                             bullet_level{.level = 2,
                                 .bullet = bullet_type::right_arrow(),
                                 .style = "Bullet Symbols"}}))
                     .first;
    doc.add(paragraph("Big").set_style(big));
    doc.add(list(list_item("one")).set_style(steps));
    (void)pic;
    odt::file(doc).save("odt/out/code_built_styles.odt");

    const auto again = odt::file::load("odt/out/code_built_styles.odt").doc;
    EXPECT_EQ(*again.styles().para.find("Big"), *doc.styles().para.find("Big"));
    EXPECT_EQ(*again.styles().frame.find("Pic"), *doc.styles().frame.find("Pic"));
    EXPECT_EQ(*again.styles().list.find("Steps"), *doc.styles().list.find("Steps"));
}

TEST(ODT, ParseHeaderRows)
{
    const text_doc doc = odt::file::load("odt/table-intermediate.odt").doc;
    const auto tables = collect<table>(doc);
    ASSERT_EQ(tables.size(), 2U);
    EXPECT_EQ(tables[0]->header_rows(), 1U);
    EXPECT_EQ(tables[0]->num_rows(), 5U) << "header rows are rows too";
    EXPECT_EQ(tables[1]->header_rows(), 0U);
}

TEST(ODT, HeaderRowsRoundTrip)
{
    table t = table::from_cells({{{"Name"}, {"Value"}}, {{"Unit"}, {"-"}}, {{"a"}, {"1"}}});
    t.set_header_rows(2);
    odt::file(text_doc{t}).save("odt/out/header_rows.odt");

    const text_doc again = odt::file::load("odt/out/header_rows.odt").doc;
    ASSERT_EQ(collect<table>(again).size(), 1U);
    EXPECT_EQ(*collect<table>(again).front(), t);
}

TEST(ODT, WhiteSpaceRoundTrips)
{
    const text_doc doc{
        paragraph("  leading, a  run,\ta tab,\na line break and trailing "),
        paragraph("a", span(" b "), " ", span("c"), "\n"),
        paragraph("log:\n  line 1\n\n\tline 3   "),
        heading(2, "\t heading"),
    };
    odt::file(doc).save("odt/out/white_space.odt");
    EXPECT_EQ(odt::file::load("odt/out/white_space.odt").doc.body, doc.body);
}

TEST(ODT, PageBreaksRoundTrip)
{
    text_doc doc;
    auto note = doc.styles().insert(paragraph_style("Note", text_props(font_style::italic))).first;
    doc.add(paragraph("one"))
        .add(page_break{})
        .add(paragraph("two").set_style(note))
        .add(page_break{})
        .add(heading(1, "three"))
        .add(page_break{})
        .add(page_break{})
        .add(table(1, 1))
        .add(page_break{})
        .add(list(list_item("in a list")))
        .add(page_break{})
        .add(paragraph()) // an empty paragraph after a break stays
        .add(page_break{});
    odt::file(doc).save("odt/out/page_breaks.odt");

    const auto again = odt::file::load("odt/out/page_breaks.odt");
    print_compare(doc, again.doc);
    EXPECT_EQ(again.doc.body, doc.body);
    // Note, and the Heading 1 the writer defined for the heading; the writer's
    // break styles aren't loaded.
    EXPECT_EQ(again.doc.styles().para.size(), 2U);
    EXPECT_NE(again.doc.styles().find(builtin::heading(1)), nullptr);

    // Saved again, same again.
    again.save("odt/out/page_breaks_resaved.odt");
    EXPECT_EQ(odt::file::load("odt/out/page_breaks_resaved.odt").doc, again.doc);
}

TEST(ODT, PageBreakBeforeLoadedParagraph)
{
    // P4, the second heading's style, is an automatic style: the break goes
    // on a copy of it.
    auto f = odt::file::load("odt/moderate.odt");
    auto pos = std::find_if(f.doc.body.begin(),
        f.doc.body.end(),
        [](const block_node &b)
        { return std::holds_alternative<heading>(b.v) && std::get<heading>(b.v).level == 2; });
    ASSERT_NE(pos, f.doc.body.end());
    f.doc.body.insert(pos, block_node{page_break{}});
    f.save("odt/out/moderate_page_break.odt");

    const auto again = odt::file::load("odt/out/moderate_page_break.odt");
    EXPECT_EQ(again.doc, f.doc);
}

TEST(ODT, PreformattedRoundTrips)
{
    const text_doc doc{
        paragraph("Run ", span("ctest").set_style(builtin::source_text), ":"),
        paragraph("$ ctest\n  1/2 Test #1: a ...   Passed\n  2/2 Test #2: b ...***Failed")
            .set_style(builtin::preformatted),
    };
    odt::file(doc).save("odt/out/preformatted.odt");

    const auto again = odt::file::load("odt/out/preformatted.odt");
    EXPECT_EQ(again.doc.body, doc.body);
    const paragraph_style *pre = again.doc.styles().find(builtin::preformatted);
    ASSERT_NE(pre, nullptr) << "the writer defines it";
    EXPECT_EQ(pre->text.m_font_size, font_size(10));
    EXPECT_NE(again.doc.styles().find(builtin::source_text), nullptr);
}

TEST(ODT, FieldsRoundTrip)
{
    text_doc doc{paragraph("Page ",
        field{field_type::page_number},
        " of ",
        field{field_type::page_count},
        ", ",
        field{field_type::title},
        ", ",
        field{field_type::date})};
    doc.meta.title = "Report";
    odt::file(doc).save("odt/out/fields.odt");
    EXPECT_EQ(odt::file::load("odt/out/fields.odt").doc.body, doc.body);
}

TEST(ODT, HeaderAndFooterRoundTrip)
{
    text_doc doc{paragraph("Body")};
    doc.meta.title = "Report";
    doc.page.set_header(paragraph(field{field_type::title}));
    doc.page.set_footer(paragraph("Page ", field{field_type::page_number}));
    odt::file(doc).save("odt/out/header_footer.odt");

    auto again = odt::file::load("odt/out/header_footer.odt");
    EXPECT_EQ(again.doc.page, doc.page);
    EXPECT_EQ(again.doc.body, doc.body);

    // Edited and cleared on a loaded file.
    again.doc.page.footer.push_back(to_block(paragraph("Confidential")));
    again.doc.page.header.clear();
    again.save("odt/out/header_footer_edited.odt");
    EXPECT_EQ(odt::file::load("odt/out/header_footer_edited.odt").doc.page, again.doc.page);
}

TEST(ODT, NoHeaderNoMasterPage)
{
    odt::file(text_doc{paragraph("Body")}).save("odt/out/no_header.odt");
    EXPECT_EQ(odt::file::load("odt/out/no_header.odt").doc.page, page_style{});
}

TEST(ODT, TableOfContentsRoundTrip)
{
    const text_doc doc{table_of_contents("Contents", 2),
        heading(1, "One"),
        heading(2, "Two"),
        heading(3, "Three")};
    odt::file(doc).save("odt/out/toc.odt");
    EXPECT_EQ(odt::file::load("odt/out/toc.odt").doc.body, doc.body);
}

TEST(ODT, LoadReportTemplate)
{
    // Made by example_make_report_template and saved again by LibreOffice.
    auto f = odt::file::load("../examples/report_template.odt");
    const paragraph header(field{field_type::title});
    ASSERT_EQ(f.doc.page.header.size(), 1U);
    EXPECT_EQ(std::get<paragraph>(f.doc.page.header.front().v).items(), header.items());
    EXPECT_FALSE(collect<field>(f.doc).empty());
    EXPECT_EQ(collect<table_of_contents>(f.doc).size(), 1U);
    EXPECT_EQ(collect<table_of_contents>(f.doc).front()->title, "Contents");
    ASSERT_NE(f.doc.styles().text.find("Pass"), nullptr);

    EXPECT_TRUE(f.doc.replace(text{"{{summary}}"}, heading(2, "Filled in")));
    EXPECT_TRUE(f.doc.replace(bookmark{"details"}, paragraph("details")));
    f.save("odt/out/report_template_filled.odt");
    const auto again = odt::file::load("odt/out/report_template_filled.odt");
    EXPECT_EQ(again.doc.body, f.doc.body);
    EXPECT_EQ(again.doc.page, f.doc.page);
}

TEST(ODT, SaveWithoutAFile)
{
    const text_doc doc{heading(1, "Saved"), paragraph("without a file")};
    odt::save(doc, "odt/out/save_free.odt");
    EXPECT_EQ(odt::file::load("odt/out/save_free.odt").doc.body, doc.body);
}
