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
#include <sstream>
#include <type_traits>

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

TEST(BASIC_USAGE, MovedFromNodeIsEmpty)
{
    span a("x");
    const span b = std::move(a);
    EXPECT_EQ(b, span("x"));
    EXPECT_TRUE(std::as_const(a).items().empty());
    EXPECT_EQ(a, span{});
    a.add("y");
    EXPECT_EQ(a, span("y"));

    // A moved-from node left in a document can still be written.
    text_doc doc{paragraph(span("z"))};
    auto &p = std::get<paragraph>(doc.body.front().v);
    const auto moved = std::move(p.items().front());
    std::ostringstream os;
    io_writer{os}(doc);
    EXPECT_NE(os.str().find("Span"), std::string::npos);
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
    EXPECT_EQ(h.items().size(), 2U);
    EXPECT_EQ(nested.items().size(), 2U);
}

TEST(BASIC_USAGE, SetTitle)
{
    text_doc doc{paragraph("Body")};
    EXPECT_EQ(doc.find_title(), nullptr);

    doc.set_title("First");
    ASSERT_NE(doc.find_title(), nullptr);
    EXPECT_EQ(doc.body.front(), to_block(title("First")));

    // A second call changes the title in place rather than adding another.
    doc.set_title("Second");
    EXPECT_EQ(doc.body.size(), 2U);
    EXPECT_EQ(*doc.find_title(), title("Second"));
    EXPECT_TRUE(doc.meta.title.empty());
}

TEST(BASIC_USAGE, FindTitleAnywhere)
{
    // A title placed by hand, after a logo, is still the title, and keeps its
    // style.
    text_doc doc{paragraph("Logo"), title("Report").set_style("Cover")};
    doc.set_title("Renamed");
    EXPECT_EQ(doc.body.size(), 2U);
    EXPECT_EQ(doc.body.back(), to_block(title("Renamed").set_style("Cover")));
}

// A title is a node of its own: not a heading, so it isn't in the outline, and
// a document can have several.
TEST(BASIC_USAGE, Titles)
{
    const text_doc doc{
        title("Report"),
        table_of_contents("Contents"),
        heading(1, "Intro"),
        title("Part ", span("two").set_style(builtin::strong)),
        heading(1, "Results"),
    };
    EXPECT_EQ(collect<title>(doc).size(), 2U);
    EXPECT_EQ(collect<heading>(doc).size(), 2U);
    EXPECT_EQ(collect<span>(doc).size(), 1U) << "walk() goes into a title";
    EXPECT_TRUE(doc.find_title()->style.empty()) << "builtin::title when written";
}

TEST(BASIC_USAGE, InsertStyle)
{
    text_doc doc;
    const text_style bold("Bold", text_props(font_weight::bold));
    EXPECT_EQ(doc.styles().insert(bold), std::pair(text_style_name("Bold"), true));
    EXPECT_EQ(*doc.styles().text.find("Bold"), bold);

    const text_style italic("Bold", text_props(font_style::italic));
    EXPECT_EQ(doc.styles().insert(italic), std::pair(text_style_name("Bold"), false));
    EXPECT_EQ(*doc.styles().text.find("Bold"), bold) << "an existing definition stays";

    EXPECT_EQ(doc.styles().insert_or_assign(italic), text_style_name("Bold"));
    EXPECT_EQ(*doc.styles().text.find("Bold"), italic);

    EXPECT_EQ(doc.styles().text.erase("Bold"), 1U);
    EXPECT_TRUE(doc.styles().empty());
}

TEST(BASIC_USAGE, SameNameInEachFamily)
{
    text_doc doc;
    doc.styles().insert(paragraph_style("Note", paragraph_props(space_after{pt(6)})));
    doc.styles().insert(text_style("Note", text_props(font_style::italic)));
    EXPECT_EQ(doc.styles().para.size(), 1U);
    EXPECT_EQ(doc.styles().text.size(), 1U);
    EXPECT_EQ(
        doc.styles().find(para_style_name("Note"))->para, paragraph_props(space_after{pt(6)}));
    EXPECT_EQ(doc.styles().find(text_style_name("Note"))->text, text_props(font_style::italic));
}

TEST(BASIC_USAGE, BuiltinStyle)
{
    text_doc doc;
    paragraph_style &body = doc.styles()[builtin::para];
    EXPECT_EQ(body.name(), builtin::para) << "added, empty";
    body.text = text_props(font_size(11));
    EXPECT_EQ(doc.styles().para.find(builtin::para)->text, text_props(font_size(11)));
    EXPECT_EQ(&doc.styles()[builtin::para], &body) << "found the second time";
    static_assert(std::is_same_v<decltype(doc.styles()[builtin::link]), text_style &>);
    static_assert(std::is_same_v<decltype(doc.styles()[builtin::table]), table_style &>);
    static_assert(std::is_same_v<decltype(doc.styles()[builtin::list]), list_style &>);
    doc.styles()[builtin::list].levels = {number_level{.level = 1}};
    EXPECT_EQ(doc.styles().list.size(), 1U);
}

TEST(BASIC_USAGE, StylePropsInPlace)
{
    paragraph_style s("Body", text_props(font_name("Liberation Serif"), font_size(11)));
    s.text.set(font_size(12)).set(font_weight::bold);
    EXPECT_EQ(s.text, text_props(font_name("Liberation Serif"), font_size(12), font_weight::bold))
        << "the rest kept";
    s.para.set(space_after{pt(6)});
    EXPECT_EQ(s.para, paragraph_props(space_after{pt(6)})) << "added";
}

TEST(BASIC_USAGE, AppendCombinesBodiesAndRegistries)
{
    text_doc doc{paragraph("first")};
    doc.styles().insert(text_style("Shared", text_props(font_weight::bold)));

    text_doc part{paragraph("second")};
    part.meta.title = "not copied";
    part.styles().insert(text_style("Shared", text_props(font_weight::bold)));
    part.styles().insert(text_style("Own", text_props(font_style::italic)));
    part.add(paragraph(frame(part.add_image("Pictures/a.png", "bytes", "image/png"))));

    doc.append(part);
    EXPECT_EQ(doc.body.size(), 3U);
    EXPECT_EQ(doc.body[1], to_block(paragraph("second")));
    EXPECT_EQ(doc.styles().text.size(), 2U);
    EXPECT_NE(doc.resources().find("Pictures/a.png"), nullptr);
    EXPECT_TRUE(doc.meta.title.empty());

    doc.append(doc);
    EXPECT_EQ(doc.body.size(), 6U);
}

TEST(BASIC_USAGE, AppendClashLeavesDocumentUnchanged)
{
    text_doc doc{paragraph("first")};
    doc.styles().insert(text_style("S", text_props(font_weight::bold)));
    const text_doc before = doc;

    text_doc part{paragraph("second")};
    part.styles().insert(text_style("Other", text_props(font_style::italic)));
    part.styles().insert(text_style("S", text_props(font_style::italic)));

    EXPECT_THROW(doc.append(part), std::invalid_argument);
    EXPECT_EQ(doc, before);
}

TEST(BASIC_USAGE, ReplacePlaceholders)
{
    text_doc doc{heading(1, "Report"),
        paragraph(" {{summary}} "),
        paragraph(bookmark{"details"}, "Details go here"),
        paragraph("Placeholder style").set_style("Placeholder")};

    EXPECT_TRUE(doc.replace(text{"{{summary}}"}, {heading(2, "Summary"), table(1, 1)}));
    EXPECT_TRUE(doc.replace(bookmark{"details"}, paragraph("details")));
    EXPECT_TRUE(doc.replace(para_style_name("Placeholder"), text_doc{}));
    EXPECT_FALSE(doc.replace(text{"{{summary}}"}, paragraph("again"))) << "already replaced";

    const text_doc expected{
        heading(1, "Report"), heading(2, "Summary"), table(1, 1), paragraph("details")};
    EXPECT_EQ(doc.body, expected.body);
}

TEST(BASIC_USAGE, StringPlaceholderIsText)
{
    text_doc doc{paragraph("{{a}}"), paragraph("{{b}}")};
    EXPECT_TRUE(doc.replace("{{a}}", paragraph("A")));
    EXPECT_TRUE(doc.replace(std::string("{{b}}"), paragraph("B")));
    const text_doc expected{paragraph("A"), paragraph("B")};
    EXPECT_EQ(doc.body, expected.body);
}

TEST(BASIC_USAGE, InsertAroundPlaceholder)
{
    text_doc doc{paragraph("a"), paragraph("{{x}}"), paragraph("b")};
    EXPECT_TRUE(doc.insert_after(text{"{{x}}"}, {paragraph("after 1"), paragraph("after 2")}));
    EXPECT_TRUE(doc.insert_before(text{"{{x}}"}, paragraph("before")));
    const text_doc expected{paragraph("a"),
        paragraph("before"),
        paragraph("{{x}}"),
        paragraph("after 1"),
        paragraph("after 2"),
        paragraph("b")};
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
    EXPECT_EQ(
        collect<table>(doc).front()->cell(0, 0).items().front(), to_block(paragraph("in a cell")));
    EXPECT_EQ(doc.page.footer.front(), to_block(paragraph("in the footer")));
}

TEST(BASIC_USAGE, FillMergesRegistries)
{
    text_doc doc{paragraph("{{x}}")};
    paragraph_style bold("Bold", text_props(font_weight::bold));

    text_doc part;
    part.add(paragraph("bold").set_style(part.styles().insert(bold).first));
    EXPECT_TRUE(doc.replace(text{"{{x}}"}, part));
    EXPECT_NE(doc.styles().para.find("Bold"), nullptr);

    // A clash throws and leaves the document as it was.
    text_doc clash{paragraph("y")};
    clash.styles().insert(paragraph_style("Bold", text_props(font_style::italic)));
    const text_doc before = doc;
    EXPECT_THROW((void)doc.insert_after(text{"bold"}, clash), std::invalid_argument);
    EXPECT_EQ(doc, before);
}

TEST(BASIC_USAGE, PropsCopyFromNonConst)
{
    text_props tp(font_size(11));
    text_props tp_copy(tp);
    paragraph_props pp(space_after{pt(6)});
    paragraph_props pp_copy(pp);
    cell_props cp(background(colour::rgb(1, 2, 3)));
    cell_props cp_copy(cp);
    table_props tbp(table_width(percent{100}));
    table_props tbp_copy(tbp);
    EXPECT_EQ(tp_copy, tp);
    EXPECT_EQ(pp_copy, pp);
    EXPECT_EQ(cp_copy, cp);
    EXPECT_EQ(tbp_copy, tbp);
}

// Content of the wrong level is rejected at the call, not deep in std::variant.
static_assert(std::is_constructible_v<paragraph, const char *, span, hyperlink>);
static_assert(!std::is_constructible_v<span, paragraph>);
static_assert(!std::is_constructible_v<paragraph, table>);
static_assert(!std::is_constructible_v<heading, int, list>);
static_assert(std::is_constructible_v<table_cell, paragraph, table>);
static_assert(!std::is_constructible_v<text_doc, span>);
static_assert(std::is_constructible_v<list, const char *, list_item>);
template <typename N, typename T>
concept can_add = requires(N n, T t) { n.add(t); };
static_assert(can_add<paragraph, text>);
static_assert(!can_add<paragraph, table>);
static_assert(!can_add<text_doc, span>);

// A property names what it is: a bare colour or percent says neither which
// colour nor what width.
static_assert(std::is_constructible_v<text_props, font_colour, font_size>);
static_assert(!std::is_constructible_v<text_props, colour>);
static_assert(!std::is_constructible_v<cell_props, colour>);
static_assert(std::is_constructible_v<cell_props, background>);
static_assert(!std::is_constructible_v<table_props, percent>);
static_assert(std::is_constructible_v<table_props, table_width>);
// A style takes its family's property sets, not single properties.
static_assert(std::is_constructible_v<paragraph_style, std::string, text_props, paragraph_props>);
static_assert(!std::is_constructible_v<paragraph_style, std::string, font_size>);
static_assert(!std::is_constructible_v<paragraph_style, std::string, cell_props>);
static_assert(std::is_constructible_v<cell_style, std::string, cell_props, text_props>);

TEST(BASIC_USAGE, ValueTypesTakeParenthesesOrBraces)
{
    EXPECT_EQ(font_size(pt(11)), font_size(11));
    EXPECT_EQ(font_name("Liberation Sans").str(), "Liberation Sans");
    EXPECT_EQ(paragraph_props(space_after(pt(6)), keep_with_next()),
        paragraph_props(space_after{pt(6)}, keep_with_next{}));
    EXPECT_EQ(
        table_props(table_width(percent(50)), outer_border(border::solid()), cell_padding(pt(2))),
        table_props(table_width(percent{50}), outer_border{border::solid()}, cell_padding{pt(2)}));
}

// A builtin's definition made with operator[] starts from its default, so
// setting one property keeps the look; a builtin without a default starts empty.
TEST(BASIC_USAGE, BuiltinDefinitionStartsFromDefault)
{
    text_doc doc;
    auto &h1 = doc.styles()[builtin::heading(1)];
    h1.text.set(font_colour(colour::rgb(200, 0, 0)));
    EXPECT_EQ(h1.text.m_font_weight, font_weight::bold);
    EXPECT_TRUE(h1.text.m_font_size.has_value());
    EXPECT_EQ(h1.parent, builtin::para);
    EXPECT_EQ(h1.text.m_font_colour, colour::rgb(200, 0, 0));

    EXPECT_EQ(doc.styles().text[builtin::link].text.m_underline, underline::single);
    EXPECT_TRUE(doc.styles().para[builtin::preformatted].text.m_font_name.has_value());

    EXPECT_EQ(doc.styles()[builtin::para], paragraph_style(builtin::para));
    EXPECT_EQ(doc.styles().para["Note"], paragraph_style("Note"));
    EXPECT_FALSE(builtin::default_style(builtin::para));
    EXPECT_FALSE(builtin::default_style(text_style_name("Note")));

    // An existing definition is returned as it is.
    doc.styles().insert_or_assign(paragraph_style(builtin::title));
    EXPECT_EQ(doc.styles()[builtin::title], paragraph_style(builtin::title));
}

template <typename Node, typename Style>
concept styleable_with = requires(Node n, const Style &s) { n.set_style(s); };

// A style stands for its name wherever a name of its family is taken.
TEST(BASIC_USAGE, StyleConvertsToItsName)
{
    const text_style bold("Bold", text_props(font_weight::bold));
    const text_style plain("Plain");
    const cell_style key("Key");
    EXPECT_EQ(span("x").set_style(bold).style, bold.name());
    EXPECT_EQ(span("x").set_style(false ? bold : plain).style, plain.name());

    table t(1, 1);
    t.set_column_style(0, key);
    EXPECT_EQ(t.column_style(0), key.name());

    static_assert(styleable_with<span, text_style>);
    static_assert(styleable_with<paragraph, paragraph_style>);
    static_assert(!styleable_with<span, paragraph_style>, "the wrong family");
    static_assert(!styleable_with<paragraph, text_style>, "the wrong family");
}

// Fluent setters on a temporary return an rvalue, so the chain is moved where
// it goes rather than copied, and can't be bound to an auto &. On a named node
// they return an lvalue, as before.
TEST(BASIC_USAGE, SettersOnTemporariesReturnRvalues)
{
    static_assert(std::is_same_v<decltype(paragraph().set_style("Note")), paragraph &&>);
    static_assert(std::is_same_v<decltype(paragraph().set_style("Note").add("x")), paragraph &&>);
    static_assert(std::is_same_v<decltype(span().add("x").set_style("S")), span &&>);
    static_assert(std::is_same_v<decltype(list().add("a")), list &&>);
    static_assert(std::is_same_v<decltype(table_cell().set_span(1, 2)), table_cell &&>);
    static_assert(
        std::is_same_v<decltype(table(1, 1).set_header_rows(1).apply_borders(border::solid())),
            table &&>);
    static_assert(std::is_same_v<decltype(text_doc().add("x").set_title("T")), text_doc &&>);
    static_assert(std::is_same_v<decltype(page_style().set_footer("f")), page_style &&>);

    paragraph p;
    static_assert(std::is_same_v<decltype(p.set_style("Note").add("x")), paragraph &>);
    text_doc doc;
    static_assert(std::is_same_v<decltype(doc.add("x")), text_doc &>);

    // A chain moves into its destination, contents intact.
    table t = table::from_cells({{"a", "b"}}).set_header_rows(1).set_column_widths({3, 1});
    EXPECT_EQ(t.header_rows(), 1U);
    EXPECT_EQ(t.column_width(0), 3.0);
    text_doc d{paragraph().set_style("Quote").add("q"), std::move(t)};
    ASSERT_EQ(d.body.size(), 2U);
    EXPECT_EQ(std::get<paragraph>(d.body[0].v).style, "Quote");
    EXPECT_EQ(std::get<table>(d.body[1].v).num_cols(), 2U);
}
