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

// Reading .docx files: round trips through the model, files made by
// LibreOffice, and load, edit, save keeping what the model doesn't hold.

#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "bytes.h"
#include "docsmithcpp/docx/file.h"
#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/odt/file.h"
#include "docx/ooxml.h"
#include "opc/package.h"

using namespace docsmith;

namespace
{
/// A block as io_writer prints it, for readable failures.
std::string tree(const block_node &b)
{
    std::ostringstream ss;
    io_writer print(ss);
    print(text_doc{b});
    return ss.str();
}

pugi::xml_document part_xml(const std::string &path, const std::string &part)
{
    const auto pkg = opc::package::read(path);
    pugi::xml_document xml;
    const auto *p = pkg.find(part);
    EXPECT_NE(p, nullptr) << part;
    if(p)
        docx::load(xml, p->data);
    return xml;
}

/// A document with every node the model has.
text_doc every_node()
{
    text_doc doc;
    auto note = doc.add_style(
        style("Note", style_family::paragraph, text_props(font_style::italic, font_size(10))));
    auto red = doc.add_style(style("Red", style_family::text, text_props(colour::rgb(200, 0, 0))));
    auto bold = doc.add_style(style("Bold", style_family::text, text_props(font_weight::bold)));
    doc.styles().find("Bold")->m_parent_style = red;
    auto steps = doc.add_style(list_style("Steps",
        {list_style_num(style{}, 1, list_enum::arabic, ".", 1),
            list_style_num(style{}, 2, list_enum::lower_roman, ")", 1, "(")}));
    auto dots = doc.add_style(list_style("Dots", {list_style_bullet(style{}, 1, "•")}));
    auto img =
        doc.add_image("word/media/smiley.png", detail::read_bytes("odt/smiley.png"), "image/png");

    doc.meta = {.title = "Every node",
        .author = "A",
        .language = "en-ZA",
        .subject = "S",
        .description = "D",
        .keywords = {"k1", "k2"}};
    doc.meta.created = std::chrono::sys_days{std::chrono::year{2025} / 1 / 2};
    doc.meta.custom["Build"] = "7";

    doc.add(paragraph("Title text").set_style(builtin::title));
    doc.add(table_of_contents("Contents", 2));
    doc.add(heading(1, "Intro"));
    doc.add(paragraph(" lead  space\tand tab\nand break ",
        span("red").set_style(red).add(span("x")),
        span("bold").set_style(bold))
            .set_style(note));
    doc.add(paragraph(bookmark{"here"},
        "a ",
        hyperlink("https://example.com/a?b=1", "url"),
        " and ",
        hyperlink(bookmark{"here"}, span("back").set_style(red))));
    doc.add(heading(2, bookmark{"lists"}, "Lists"));
    doc.add(list(list_item(paragraph("one"), list(list_item("one.i"), list_item("one.ii"))),
        list_item("two", "two, more"))
            .set_style(steps));
    doc.add(list(list_item("dot")).set_style(dots));
    doc.add(list(list_item("unstyled")));
    // Numbering that carries on from the Steps list above, and restarts at 5.
    list more(list_item("three"), list_item("five"), list_item("six"));
    more.set_style(steps);
    more.continue_numbering = true;
    more.items()[1].start_value = 5;
    doc.add(more);
    doc.add(page_break{});
    doc.add(heading(2, "Table"));
    table t(3, 3);
    t.cell(0, 0).set_span(1, 2).add("wide");
    t.cell(0, 2).add("h");
    t.cell(1, 0).set_span(2, 1).add("tall");
    t.cell(1, 1).add(paragraph("x")).add(paragraph("y"));
    t.cell(2, 2).add(table::from_cells({{{"n"}}}));
    t.set_header_rows(1);
    doc.add(t);
    // Read back with the size written, so it has one.
    frame sized(img);
    sized.width = pt(36);
    sized.height = pt(24);
    doc.add(sized);
    doc.add(paragraph("inline ", sized, " image"));
    doc.add(paragraph("int main()\n{\n    return 0;\n}").set_style(builtin::preformatted));
    doc.add(paragraph("call ", span("f()").set_style(builtin::source_text)));
    doc.add(
        unknown{R"(<w:customXml w:element="x"><w:p><w:r><w:t>raw</w:t></w:r></w:p></w:customXml>)",
            markup::ooxml});
    doc.page.set_header(paragraph(field{field_type::title}));
    doc.page.set_footer(paragraph("Page ",
        field{field_type::page_number},
        " of ",
        field{field_type::page_count},
        " on ",
        field{field_type::date}));
    return doc;
}
} // namespace

TEST(DOCX_Read, RoundTripThroughTheModel)
{
    const text_doc doc = every_node();
    docx::file(doc).save("docx/out/every_node.docx");
    const text_doc back = docx::file::load("docx/out/every_node.docx").doc;

    // A span in a span is written with the inner style: runs don't nest.
    text_doc expected = doc;
    auto &lead = std::get<paragraph>(expected.body[3].v);
    lead.children()[1] = inline_node{span("red").set_style("Red")};
    lead.children().insert(lead.children().begin() + 2, inline_node{span("x").set_style("Red")});
    // Adjacent runs of one style read back as one span.
    lead.children()[1] = inline_node{span("redx").set_style("Red")};
    lead.children().erase(lead.children().begin() + 2);

    ASSERT_EQ(back.body.size(), expected.body.size());
    for(std::size_t i = 0; i != back.body.size(); ++i)
        EXPECT_EQ(back.body[i], expected.body[i]) << "block " << i << "\n"
                                                  << tree(back.body[i]) << "expected\n"
                                                  << tree(expected.body[i]);
    EXPECT_EQ(back.page, expected.page);
    EXPECT_EQ(back.meta, expected.meta);

    for(const auto &[name, s] : expected.styles())
    {
        ASSERT_NE(back.styles().find(name), nullptr) << name;
        EXPECT_EQ(*back.styles().find(name), s) << name;
    }
    for(const auto &[name, ls] : expected.list_styles())
    {
        ASSERT_NE(back.list_styles().find(name), nullptr) << name;
        EXPECT_EQ(*back.list_styles().find(name), ls) << name;
    }
    EXPECT_EQ(back.list_styles().find(docx::default_list_name), nullptr);
    EXPECT_EQ(back.resources(), expected.resources());
}

TEST(DOCX_Read, ResaveIsStable)
{
    // A loaded file saved again reads back the same, and saving it again
    // doesn't grow it: old numbering instances and relationships are dropped.
    docx::file(every_node()).save("docx/out/resave_1.docx");
    const auto first = docx::file::load("docx/out/resave_1.docx");
    first.save("docx/out/resave_2.docx");
    const auto second = docx::file::load("docx/out/resave_2.docx");
    EXPECT_EQ(second.doc, first.doc);
    second.save("docx/out/resave_3.docx");

    const auto nums = [](const std::string &path)
    {
        return part_xml(path, "word/numbering.xml")
            .child("w:numbering")
            .select_nodes("w:num")
            .size();
    };
    EXPECT_EQ(nums("docx/out/resave_3.docx"), nums("docx/out/resave_1.docx"));
    EXPECT_EQ(opc::package::read("docx/out/resave_3.docx").rels("word/document.xml"),
        opc::package::read("docx/out/resave_1.docx").rels("word/document.xml"));
}

TEST(DOCX_Read, KeepsWhatTheModelDoesntHold)
{
    // As another application might write it: its own prefix for
    // WordprocessingML, direct formatting, a footnote, a landscape page and a
    // theme part.
    opc::package pkg;
    pkg.set("word/document.xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<ns0:document xmlns:ns0="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
    xmlns:rel="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><ns0:body>
<ns0:p><ns0:pPr><ns0:jc ns0:val="center"/></ns0:pPr><ns0:r><ns0:rPr><ns0:b/><ns0:u ns0:val="double"/></ns0:rPr><ns0:t>Bold and double underlined</ns0:t></ns0:r><ns0:r><ns0:t xml:space="preserve"> plain</ns0:t></ns0:r><ns0:r><ns0:footnoteReference ns0:id="1"/></ns0:r></ns0:p>
<ns0:sectPr><ns0:pgSz ns0:w="16838" ns0:h="11906" ns0:orient="landscape"/></ns0:sectPr>
</ns0:body></ns0:document>)",
        docx::content_type::document);
    pkg.set("word/theme/theme1.xml",
        "<a:theme xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\"/>",
        "application/vnd.openxmlformats-officedocument.theme+xml");
    pkg.set("word/footnotes.xml",
        R"(<w:footnotes xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">)"
        R"(<w:footnote w:id="1"><w:p><w:r><w:t>A note.</w:t></w:r></w:p></w:footnote></w:footnotes>)",
        "application/vnd.openxmlformats-officedocument.wordprocessingml.footnotes+xml");
    pkg.ensure_rel("", docx::rel::office_document, "word/document.xml");
    pkg.ensure_rel("word/document.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/footnotes",
        "footnotes.xml");
    pkg.ensure_rel("word/document.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme",
        "theme/theme1.xml");
    pkg.write("docx/out/foreign.docx");

    auto f = docx::file::load("docx/out/foreign.docx");
    ASSERT_EQ(f.doc.body.size(), 1U);
    const auto &p = std::get<paragraph>(f.doc.body[0].v);
    ASSERT_FALSE(p.style.is_empty()) << "the paragraph's alignment is its own";
    const style *ps = f.doc.styles().find(p.style.get_name());
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->m_family, style_family::paragraph);

    ASSERT_EQ(p.children().size(), 3U);
    const auto &bold = std::get<span>(p.children()[0].v);
    const style *bs = f.doc.styles().find(bold.style.get_name());
    ASSERT_NE(bs, nullptr);
    EXPECT_EQ(bs->m_text_props->m_font_weight, font_weight::bold)
        << "direct formatting is a style the model reads";
    EXPECT_EQ(std::get<text>(p.children()[1].v).content, " plain");
    EXPECT_NE(
        std::get<unknown>(p.children()[2].v).xml.find("w:footnoteReference"), std::string::npos);

    f.doc.add(paragraph("added"));
    f.save("docx/out/foreign_resaved.docx");

    const auto out = opc::package::read("docx/out/foreign_resaved.docx");
    EXPECT_TRUE(out.contains("word/theme/theme1.xml"));
    EXPECT_TRUE(out.contains("word/footnotes.xml"));
    const auto xml = part_xml("docx/out/foreign_resaved.docx", "word/document.xml");
    auto body = xml.child("w:document").child("w:body");
    auto first = body.child("w:p");
    EXPECT_EQ(docx::val(first.child("w:pPr"), "w:jc"), "center");
    EXPECT_EQ(docx::val(first.child("w:r").child("w:rPr"), "w:u"), "double")
        << "kept, though the model has no underline";
    EXPECT_FALSE(xml::serialize(xml).find("<w:style") != std::string::npos);
    EXPECT_TRUE(first.select_node(".//w:footnoteReference"));
    EXPECT_STREQ(
        body.child("w:sectPr").child("w:pgSz").attribute("w:orient").as_string(), "landscape");
    EXPECT_STREQ(
        body.child("w:p").next_sibling("w:p").child("w:r").child("w:t").text().get(), "added");

    // Direct formatting stays direct: it isn't added to styles.xml.
    const auto styles = part_xml("docx/out/foreign_resaved.docx", "word/styles.xml");
    EXPECT_FALSE(styles.child("w:styles")
            .find_child_by_attribute("w:style", "w:styleId", bold.style.get_name().c_str()));
}

TEST(DOCX_Read, LibreOfficeListsLinksAndStyles)
{
    const auto f = docx::file::load("docx/moderate.docx");
    const auto &body = f.doc.body;
    ASSERT_GE(body.size(), 5U);
    const auto &h1 = std::get<heading>(body[0].v);
    EXPECT_EQ(h1.level, 1);
    EXPECT_TRUE(h1.style.is_empty()) << "Heading1 is a level 1 heading's default";
    EXPECT_EQ(std::get<paragraph>(body[1].v).style.get_name(), "TextBody")
        << "LibreOffice's direct formatting that only repeats the style is not the paragraph's own";

    const auto lists = collect<list>(f.doc);
    ASSERT_GE(lists.size(), 3U);
    const list_style *numbered = f.doc.list_styles().find(lists[0]->style.get_name());
    ASSERT_NE(numbered, nullptr);
    const auto &level1 = std::get<list_style_num>(numbered->m_level_styles.front());
    EXPECT_EQ(level1.m_start_from, 7);
    EXPECT_EQ(lists[0]->items().size(), 2U);

    const auto links = collect<hyperlink>(f.doc);
    ASSERT_EQ(links.size(), 1U);
    EXPECT_EQ(std::get<url_target>(links[0]->target).url,
        "https://github.com/MichaelCoutlakis/docsmithcpp");
    EXPECT_FALSE(find_all<span>(f.doc,
        [](const span &s)
        {
            return s.style == builtin::source_text;
        }).empty())
        << "LibreOffice's Source Text is the builtin";
}

TEST(DOCX_Read, LibreOfficeHeaderFooterFieldsAndToc)
{
    auto f = docx::file::load("docx/report_template.docx");
    const auto tocs = collect<table_of_contents>(f.doc);
    ASSERT_EQ(tocs.size(), 1U);
    EXPECT_EQ(tocs[0]->title, "Contents");
    EXPECT_EQ(tocs[0]->levels, 2);
    EXPECT_TRUE(collect<bookmark>(f.doc).size() == 1U)
        << "only 'details': LibreOffice's _toc bookmarks are dropped";

    auto has_field = [](const std::vector<block_node> &blocks, field_type t)
    {
        text_doc d;
        d.body = blocks;
        return !find_all<field>(d, [t](const field &x) { return x.type == t; }).empty();
    };
    EXPECT_TRUE(has_field(f.doc.page.header, field_type::title));
    EXPECT_TRUE(has_field(f.doc.page.footer, field_type::page_number));
    EXPECT_TRUE(has_field(f.doc.page.footer, field_type::page_count));
    EXPECT_TRUE(has_field(f.doc.body, field_type::date));

    // Filling it in and saving as .docx and .odt.
    EXPECT_TRUE(f.doc.replace(text{"{{summary}}"}, {paragraph("All passed.")}));
    EXPECT_TRUE(f.doc.insert_after(bookmark{"details"}, {heading(2, "case a")}));
    f.save("docx/out/report_filled.docx");
    odt::file(f.doc).save("docx/out/report_filled.odt");
    const auto back = docx::file::load("docx/out/report_filled.docx");
    EXPECT_EQ(back.doc.body, f.doc.body);
    EXPECT_EQ(back.doc.page, f.doc.page);
    const auto odt = odt::file::load("docx/out/report_filled.odt");
    EXPECT_EQ(collect<table_of_contents>(odt.doc).size(), 1U);
}

TEST(DOCX_Read, LibreOfficeTable)
{
    const auto f = docx::file::load("docx/table-intermediate.docx");
    const auto tables = collect<table>(f.doc);
    ASSERT_EQ(tables.size(), 2U);
    const table &t = *tables[0];
    EXPECT_EQ(t.num_rows(), 5U);
    EXPECT_EQ(t.num_cols(), 3U);
    EXPECT_EQ(t.header_rows(), 1U);
    EXPECT_EQ(t.cell(1, 0).rowspan, 2);
    EXPECT_EQ(t.cell(1, 0).colspan, 2);
    EXPECT_TRUE(t.is_cell_covered(2, 1));
}

TEST(DOCX_Read, LibreOfficeImage)
{
    const auto f = docx::file::load("docx/image.docx");
    const auto frames = collect<frame>(f.doc);
    ASSERT_EQ(frames.size(), 1U);
    const resource *r = f.doc.resources().find(frames[0]->img.uri);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->media_type(), "image/png");
    EXPECT_FALSE(r->data().empty());

    // The drawing (here anchored, not inline) is written back as it was.
    f.save("docx/out/image_resaved.docx");
    const auto xml = part_xml("docx/out/image_resaved.docx", "word/document.xml");
    EXPECT_TRUE(xml.select_node("//wp:anchor"));
    EXPECT_TRUE(opc::package::read("docx/out/image_resaved.docx").contains(frames[0]->img.uri));
}

TEST(DOCX_Read, NotAWordDocument)
{ EXPECT_THROW(docx::file::load("odt/basic.odt"), std::runtime_error); }

TEST(DOCX_Read, TableFormattingRoundTrip)
{
    text_doc doc;
    const border thick = border::solid(pt(1.5), colour::rgb(0x20, 0x40, 0x60));
    const border thin = border::solid(pt(0.5));
    doc.add_style(style("Grid",
        style_family::table,
        table_props(outer_border{thick}, inside_h{thin}, inside_v{thin}, cell_padding{pt(3)})));
    table t = table::from_cells({{"Test", "Result"}, {"open", "pass"}, {"save", "fail"}});
    t.set_style("Grid").set_column_widths({3, 1});
    t.cell(1, 1).props.set(colour::rgb(0xff, 0xcc, 0xcc)).set(cell_valign::middle);
    t.set_row_borders({0}, edge::bottom, border::none());
    doc.add(t);
    docx::file(doc).save("docx/out/table_formatting_read.docx");

    const auto f = docx::file::load("docx/out/table_formatting_read.docx");
    const table &back = *collect<table>(f.doc).front();
    EXPECT_EQ(back.style, style_name("Grid"));
    const style *grid = f.doc.styles().find("Grid");
    ASSERT_NE(grid, nullptr);
    ASSERT_TRUE(grid->m_table_props);
    EXPECT_EQ(grid->m_table_props->m_outer, (cell_borders{thick, thick, thick, thick}));
    EXPECT_EQ(grid->m_table_props->m_inside_h, thin);
    EXPECT_EQ(grid->m_table_props->m_padding, pt(3));
    EXPECT_NEAR(column_shares(back).at(0), 0.75, 1e-3) << "widths are whole twips";
    for(size_t r = 0; r != t.num_rows(); ++r)
        for(size_t c = 0; c != t.num_cols(); ++c)
            EXPECT_EQ(effective_cell_props(back, f.doc.styles(), r, c),
                effective_cell_props(t, doc.styles(), r, c))
                << r << ", " << c;
}
