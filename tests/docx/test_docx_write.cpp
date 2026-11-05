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

// White-box tests of the DOCX writer: what it puts in each part.

#include <filesystem>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "bytes.h"
#include "docsmithcpp/docx/file.h"
#include "docx/ooxml.h"
#include "opc/package.h"

using namespace docsmith;

namespace
{
/// A saved document's package, and one of its parts as XML.
struct saved
{
    saved(const text_doc &doc, const std::string &name) :
        path("docx/out/" + name + ".docx")
    {
        docx::file(doc).save(path);
        pkg = opc::package::read(path);
    }

    pugi::xml_document xml(const std::string &part) const
    {
        pugi::xml_document out;
        const auto *p = pkg.find(part);
        EXPECT_NE(p, nullptr) << part;
        if(p)
            EXPECT_TRUE(docx::load(out, p->data)) << part;
        return out;
    }

    pugi::xml_node body()
    { return (m_document = xml("word/document.xml")).child("w:document").child("w:body"); }

    std::string path;
    opc::package pkg;

private:
    pugi::xml_document m_document;
};

std::string str(const pugi::xml_node &n)
{
    std::ostringstream ss;
    n.print(ss, "", pugi::format_raw);
    return ss.str();
}

pugi::xml_node style_def(const pugi::xml_document &styles, const std::string &id)
{ return styles.child("w:styles").find_child_by_attribute("w:style", "w:styleId", id.c_str()); }
} // namespace

TEST(DOCX_Write, Package)
{
    saved s(text_doc{paragraph("Hello")}, "package");
    EXPECT_EQ(s.pkg.find("word/document.xml")->content_type, docx::content_type::document);
    const auto *main = s.pkg.find_rel_by_type("", docx::rel::office_document);
    ASSERT_NE(main, nullptr);
    EXPECT_EQ(main->target, "word/document.xml");
    EXPECT_NE(s.pkg.find_rel_by_type("word/document.xml", docx::rel::styles), nullptr);
    EXPECT_NE(s.pkg.find_rel_by_type("word/document.xml", docx::rel::settings), nullptr);
    EXPECT_FALSE(s.pkg.contains("word/numbering.xml")) << "no lists, no numbering";
    EXPECT_EQ(str(s.body().child("w:p")), "<w:p><w:r><w:t>Hello</w:t></w:r></w:p>");
}

TEST(DOCX_Write, ParagraphsAndHeadings)
{
    text_doc doc;
    auto note = doc.add_style(
        style("Note_20_Text", style_family::paragraph, text_props(font_style::italic)));
    doc.add(heading(2, "Results")).add(paragraph("text").set_style(note));
    saved s(doc, "headings");
    auto body = s.body();
    auto h = body.child("w:p");
    EXPECT_EQ(docx::val(h.child("w:pPr"), "w:pStyle"), "Heading2");
    EXPECT_EQ(docx::val(h.child("w:pPr"), "w:outlineLvl"), "1");
    EXPECT_EQ(docx::val(h.next_sibling().child("w:pPr"), "w:pStyle"), "Note_20_Text");

    const auto styles = s.xml("word/styles.xml");
    auto h2 = style_def(styles, "Heading2");
    EXPECT_EQ(docx::val(h2.child("w:pPr"), "w:outlineLvl"), "1")
        << "the default heading style is defined";
    auto def = style_def(styles, "Note_20_Text");
    EXPECT_EQ(docx::val(def, "w:name"), "Note Text");
    EXPECT_TRUE(def.child("w:rPr").child("w:i"));
}

TEST(DOCX_Write, WhiteSpaceTabsAndBreaks)
{
    saved s(text_doc{paragraph(" a  b\tc\nd")}, "white_space");
    EXPECT_EQ(str(s.body().child("w:p").child("w:r")),
        R"(<w:r><w:t xml:space="preserve"> a  b</w:t><w:tab/><w:t>c</w:t><w:br/><w:t>d</w:t></w:r>)");
}

TEST(DOCX_Write, SpansAndStyles)
{
    text_doc doc;
    auto red = doc.add_style(style("Red", text_props(colour::rgb(255, 0, 0), font_size(11))));
    auto both = doc.add_style(style("Both", text_props(font_weight::bold)));
    doc.add(
        paragraph("a ", span("red").set_style(red), span("both").set_style(both)).set_style(both));
    saved s(doc, "spans");
    auto p = s.body().child("w:p");
    EXPECT_EQ(docx::val(p.child("w:pPr"), "w:pStyle"), "Both");
    auto runs = p.children("w:r");
    auto it = runs.begin();
    EXPECT_FALSE(it->child("w:rPr"));
    EXPECT_EQ(docx::val((++it)->child("w:rPr"), "w:rStyle"), "Red");
    EXPECT_EQ(docx::val((++it)->child("w:rPr"), "w:rStyle"), "BothChar")
        << "a style used on paragraphs and spans is written twice, linked";

    const auto styles = s.xml("word/styles.xml");
    auto r = style_def(styles, "Red");
    EXPECT_STREQ(r.attribute("w:type").as_string(), "character");
    EXPECT_EQ(docx::val(r.child("w:rPr"), "w:color"), "FF0000");
    EXPECT_EQ(docx::val(r.child("w:rPr"), "w:sz"), "22");
    EXPECT_STREQ(style_def(styles, "Both").attribute("w:type").as_string(), "paragraph");
    EXPECT_EQ(docx::val(style_def(styles, "BothChar"), "w:link"), "Both");
}

TEST(DOCX_Write, LinksAndBookmarks)
{
    saved s(text_doc{paragraph(bookmark{"top"},
                hyperlink("https://example.com", "site"),
                " ",
                hyperlink(bookmark{"top"}, "back"))},
        "links");
    auto p = s.body().child("w:p");
    auto start = p.child("w:bookmarkStart");
    EXPECT_STREQ(start.attribute("w:name").as_string(), "top");
    EXPECT_STREQ(p.child("w:bookmarkEnd").attribute("w:id").as_string(),
        start.attribute("w:id").as_string());

    auto links = p.children("w:hyperlink");
    auto url = *links.begin();
    const auto *r = s.pkg.find_rel("word/document.xml", url.attribute("r:id").as_string());
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->target, "https://example.com");
    EXPECT_TRUE(r->external);
    EXPECT_EQ(docx::val(url.child("w:r").child("w:rPr"), "w:rStyle"), "Hyperlink");
    EXPECT_STREQ((*++links.begin()).attribute("w:anchor").as_string(), "top");
    EXPECT_TRUE(style_def(s.xml("word/styles.xml"), "Hyperlink"));
}

TEST(DOCX_Write, Lists)
{
    text_doc doc;
    const style plain("Plain");
    auto numbered = doc.add_style(list_style("Numbered",
        {list_style_num(plain, 1, list_enum::arabic, ".", 3),
            list_style_num(plain, 2, list_enum::lower_alpha, ")")}));
    doc.add(list(list_item(paragraph("one"), list(list_item("one.a"))), list_item("two", "more"))
            .set_style(numbered));
    doc.add(list(list_item("bullet")));
    saved s(doc, "lists");

    std::vector<std::pair<std::string, std::string>> items; // ilvl, numId
    for(auto p : s.body().children("w:p"))
    {
        auto num = p.child("w:pPr").child("w:numPr");
        items.emplace_back(docx::val(num, "w:ilvl"), docx::val(num, "w:numId"));
    }
    ASSERT_EQ(items.size(), 5U);
    EXPECT_EQ(items[0].first, "0");
    EXPECT_EQ(items[1].first, "1");
    EXPECT_EQ(items[1].second, items[0].second) << "an unstyled nested list continues its parent's";
    EXPECT_EQ(items[2], items[0]);
    EXPECT_EQ(items[3].second, "0") << "a later paragraph of an item";
    EXPECT_NE(items[4].second, items[0].second);

    const auto numbering = s.xml("word/numbering.xml");
    auto root = numbering.child("w:numbering");
    auto abstract = root.find_child_by_attribute("w:abstractNum",
        "w:abstractNumId",
        docx::val(root.find_child_by_attribute("w:num", "w:numId", items[0].second.c_str()),
            "w:abstractNumId")
            .c_str());
    EXPECT_EQ(docx::val(abstract, "w:name"), "Numbered");
    auto lvl = abstract.child("w:lvl");
    EXPECT_EQ(docx::val(lvl, "w:numFmt"), "decimal");
    EXPECT_EQ(docx::val(lvl, "w:lvlText"), "%1.");
    EXPECT_EQ(docx::val(lvl, "w:start"), "3");
    EXPECT_EQ(docx::val(lvl.next_sibling(), "w:lvlText"), "%2)");
    EXPECT_STREQ(root.first_child().name(), "w:abstractNum") << "definitions come before instances";
}

TEST(DOCX_Write, TableSpansAndHeaderRows)
{
    table t(3, 3);
    t.cell(0, 0).set_span(1, 2).add("wide");
    t.cell(1, 0).set_span(2, 1).add("tall");
    t.cell(1, 1).add("x");
    t.set_header_rows(1);
    saved s(text_doc{t}, "table");
    auto tbl = s.body().child("w:tbl");
    EXPECT_EQ(std::distance(tbl.child("w:tblGrid").children("w:gridCol").begin(),
                  tbl.child("w:tblGrid").children("w:gridCol").end()),
        3);
    auto rows = tbl.children("w:tr");
    auto r0 = *rows.begin();
    EXPECT_TRUE(r0.child("w:trPr").child("w:tblHeader"));
    EXPECT_EQ(docx::val(r0.child("w:tc").child("w:tcPr"), "w:gridSpan"), "2");
    EXPECT_EQ(std::distance(r0.children("w:tc").begin(), r0.children("w:tc").end()), 2);
    auto r1 = r0.next_sibling("w:tr");
    EXPECT_FALSE(r1.child("w:trPr"));
    EXPECT_EQ(docx::val(r1.child("w:tc").child("w:tcPr"), "w:vMerge"), "restart");
    auto r2 = r1.next_sibling("w:tr");
    auto merged = r2.child("w:tc").child("w:tcPr").child("w:vMerge");
    EXPECT_TRUE(merged);
    EXPECT_FALSE(merged.attribute("w:val")) << "continue";
    for(auto tr : rows)
        for(auto tc : tr.children("w:tc"))
            EXPECT_TRUE(tc.child("w:p")) << "every cell has a paragraph";
}

TEST(DOCX_Write, Images)
{
    text_doc doc;
    const std::string png = detail::read_bytes("odt/smiley.png");
    auto img = doc.add_image("Pictures/smiley.png", png, "image/png");
    doc.add(frame(img)).add(paragraph("inline ", frame(img)));
    saved s(doc, "images");
    ASSERT_TRUE(s.pkg.contains("word/media/smiley.png"));
    EXPECT_EQ(s.pkg.find("word/media/smiley.png")->content_type, "image/png");

    auto body = s.body();
    auto drawing = body.child("w:p").child("w:r").child("w:drawing");
    auto extent = drawing.child("wp:inline").child("wp:extent");
    EXPECT_GT(extent.attribute("cx").as_llong(), 0);
    EXPECT_GT(extent.attribute("cy").as_llong(), 0);
    auto blip = drawing.select_node(".//a:blip").node();
    const auto *r = s.pkg.find_rel("word/document.xml", blip.attribute("r:embed").as_string());
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->target, "media/smiley.png");

    auto second = body.child("w:p").next_sibling("w:p").select_node(".//wp:docPr").node();
    EXPECT_NE(second.attribute("id").as_int(),
        drawing.select_node(".//wp:docPr").node().attribute("id").as_int());
    EXPECT_EQ(std::distance(
                  s.pkg.rels("word/document.xml").begin(), s.pkg.rels("word/document.xml").end()),
        3)
        << "one image relationship for both, plus styles and settings";
}

TEST(DOCX_Write, PageBreak)
{
    saved s(text_doc{paragraph("one"), page_break{}, paragraph("two")}, "page_break");
    auto p = s.body().child("w:p").next_sibling("w:p");
    EXPECT_EQ(str(p), R"(<w:p><w:r><w:br w:type="page"/></w:r></w:p>)");
}

TEST(DOCX_Write, CodeBlocks)
{
    saved s(text_doc{paragraph("int x;\nint y;").set_style(builtin::preformatted),
                paragraph("call ", span("f()").set_style(builtin::source_text))},
        "code");
    const auto styles = s.xml("word/styles.xml");
    auto pre = style_def(styles, builtin::preformatted.get_name());
    EXPECT_EQ(docx::val(pre, "w:name"), "Preformatted Text");
    EXPECT_STREQ(
        pre.child("w:rPr").child("w:rFonts").attribute("w:ascii").as_string(), "Courier New");
    auto src = style_def(styles, builtin::source_text.get_name());
    EXPECT_STREQ(src.attribute("w:type").as_string(), "character");
}

TEST(DOCX_Write, HeaderFooterAndFields)
{
    text_doc doc{paragraph("body")};
    doc.meta.title = "Report";
    doc.page.set_header(paragraph(field{field_type::title}));
    doc.page.set_footer(paragraph("Page ",
        field{field_type::page_number},
        " of ",
        field{field_type::page_count},
        ", ",
        field{field_type::date}));
    saved s(doc, "header_footer");

    auto sect = s.body().child("w:sectPr");
    auto header_ref = sect.child("w:headerReference");
    auto footer_ref = sect.child("w:footerReference");
    ASSERT_TRUE(header_ref);
    ASSERT_TRUE(footer_ref);
    EXPECT_EQ(header_ref.next_sibling(), footer_ref);
    const auto *h = s.pkg.find_rel("word/document.xml", header_ref.attribute("r:id").as_string());
    ASSERT_NE(h, nullptr);
    const auto header = s.xml(opc::package::resolve("word/document.xml", h->target));
    auto title = header.child("w:hdr").child("w:p").child("w:fldSimple");
    EXPECT_STREQ(title.attribute("w:instr").as_string(), " TITLE ");
    EXPECT_STREQ(title.child("w:r").child("w:t").text().get(), "Report");

    const auto *f = s.pkg.find_rel("word/document.xml", footer_ref.attribute("r:id").as_string());
    const auto footer = s.xml(opc::package::resolve("word/document.xml", f->target));
    std::vector<std::string> instrs;
    for(auto fld : footer.child("w:ftr").child("w:p").children("w:fldSimple"))
        instrs.emplace_back(fld.attribute("w:instr").as_string());
    EXPECT_EQ(
        instrs, (std::vector<std::string>{" PAGE ", " NUMPAGES ", " DATE \\@ \"yyyy-MM-dd\" "}));
}

TEST(DOCX_Write, TableOfContents)
{
    text_doc doc{table_of_contents("Contents", 2),
        heading(1, "Intro"),
        heading(2, bookmark{"details"}, "Details"),
        heading(3, "Too deep")};
    saved s(doc, "toc");
    auto body = s.body();
    auto sdt = body.child("w:sdt");
    EXPECT_EQ(docx::val(sdt.child("w:sdtPr").child("w:docPartObj"), "w:docPartGallery"),
        "Table of Contents");
    auto content = sdt.child("w:sdtContent");
    auto title = content.child("w:p");
    EXPECT_EQ(docx::val(title.child("w:pPr"), "w:pStyle"), "TOCHeading");
    EXPECT_STREQ(
        content.select_node(".//w:instrText").node().text().get(), " TOC \\o \"1-2\" \\h \\z \\u ");

    std::vector<std::string> anchors;
    for(auto link : content.select_nodes(".//w:hyperlink"))
        anchors.emplace_back(link.node().attribute("w:anchor").as_string());
    ASSERT_EQ(anchors.size(), 2U);
    EXPECT_EQ(anchors[1], "details") << "a heading's own bookmark";
    auto intro = sdt.next_sibling("w:p");
    EXPECT_STREQ(
        intro.child("w:bookmarkStart").attribute("w:name").as_string(), anchors[0].c_str());
    EXPECT_TRUE(anchors[0].starts_with("_Toc"));

    const auto settings = s.xml("word/settings.xml");
    EXPECT_STREQ(
        settings.child("w:settings").child("w:updateFields").attribute("w:val").as_string(),
        "true");
    EXPECT_TRUE(style_def(s.xml("word/styles.xml"), "TOC2"));
}

TEST(DOCX_Write, Metadata)
{
    text_doc doc{paragraph("x")};
    doc.meta = {
        .title = "T", .author = "A. Writer", .language = "en-ZA", .keywords = {"one", "two"}};
    doc.meta.created =
        std::chrono::sys_days{std::chrono::year{2025} / 6 / 16} + std::chrono::hours{13};
    doc.meta.custom["Build"] = "42";
    saved s(doc, "meta");
    const auto core = s.xml("docProps/core.xml");
    auto root = core.child("cp:coreProperties");
    EXPECT_STREQ(root.child("dc:title").text().get(), "T");
    EXPECT_STREQ(root.child("dc:creator").text().get(), "A. Writer");
    EXPECT_STREQ(root.child("cp:keywords").text().get(), "one, two");
    EXPECT_STREQ(root.child("dcterms:created").text().get(), "2025-06-16T13:00:00Z");
    EXPECT_STREQ(root.child("dcterms:created").attribute("xsi:type").as_string(), "dcterms:W3CDTF");
    const auto custom = s.xml("docProps/custom.xml");
    auto prop = custom.child("cup:Properties").child("cup:property");
    EXPECT_STREQ(prop.attribute("name").as_string(), "Build");
    EXPECT_STREQ(prop.child("vt:lpwstr").text().get(), "42");
    EXPECT_TRUE(s.pkg.contains("docProps/app.xml"));
}

TEST(DOCX_Write, UnknownFromAnotherFormatIsDropped)
{
    text_doc doc;
    doc.add(unknown{"<text:sequence-decls/>"});
    doc.add(unknown{"<w:p><w:r><w:t>kept</w:t></w:r></w:p>", markup::ooxml});
    saved s(doc, "unknown");
    auto body = s.body();
    EXPECT_STREQ(body.first_child().name(), "w:p");
    EXPECT_STREQ(body.first_child().child("w:r").child("w:t").text().get(), "kept");
    EXPECT_STREQ(body.first_child().next_sibling().name(), "w:sectPr");
}

// DOCX has no cell styles: a column's style is written on its cells, and its
// paragraph style on their paragraphs.
TEST(DOCX_Write, ColumnStyle)
{
    text_doc doc;
    doc.add_style(style("Key text", style_family::paragraph, text_props(font_weight::bold)));
    doc.add_style(style("Key",
        style_family::table_cell,
        cell_props(colour::rgb(0xee, 0xee, 0xee), paragraph_style{"Key text"})));
    table t = table::from_cells({{"a", "1"}, {"b", "2"}});
    t.set_column_style(0, "Key");
    doc.add(t);
    saved s(doc, "column_style");

    const auto rows = s.body().child("w:tbl").children("w:tr");
    for(auto tr : rows)
    {
        auto first = tr.child("w:tc");
        EXPECT_STREQ(first.child("w:tcPr").child("w:shd").attribute("w:fill").as_string(), "eeeeee");
        EXPECT_STREQ(
            first.child("w:p").child("w:pPr").child("w:pStyle").attribute("w:val").as_string(),
            "Key text");
        EXPECT_FALSE(first.next_sibling("w:tc").child("w:p").child("w:pPr").child("w:pStyle"));
    }
}

TEST(DOCX_Write, TableFormatting)
{
    text_doc doc;
    const border thick = border::solid(pt(1.5), colour::rgb(0x20, 0x40, 0x60));
    const border thin = border::solid(pt(0.5));
    doc.add_style(style("Grid",
        style_family::table,
        table_props(percent{80},
            table_align::centre,
            outer_border{thick},
            inside_h{thin},
            cell_padding{pt(3)},
            header_cells{cell_props(colour::rgb(0xdd, 0xdd, 0xdd), cell_borders{.m_bottom = thick})})));
    table t = table::from_cells({{"Test", "Result"}, {"open", "pass"}});
    t.set_header_rows(1).set_style("Grid").set_column_widths({3, 1});
    t.cell(1, 1).props.set(colour::rgb(0xff, 0xcc, 0xcc));
    doc.add(t);
    saved s(doc, "table_formatting");

    // The lines and padding are in the table style, as in Word's Table Grid.
    const auto styles = s.xml("word/styles.xml");
    auto grid = style_def(styles, "Grid");
    EXPECT_STREQ(grid.attribute("w:type").as_string(), "table");
    auto borders = grid.child("w:tblPr").child("w:tblBorders");
    EXPECT_STREQ(borders.child("w:top").attribute("w:val").as_string(), "single");
    EXPECT_EQ(borders.child("w:top").attribute("w:sz").as_int(), 12);
    EXPECT_STREQ(borders.child("w:top").attribute("w:color").as_string(), "204060");
    EXPECT_EQ(borders.child("w:insideH").attribute("w:sz").as_int(), 4);
    EXPECT_FALSE(borders.child("w:insideV"));
    EXPECT_EQ(
        grid.child("w:tblPr").child("w:tblCellMar").child("w:left").attribute("w:w").as_int(), 60);

    auto tbl = s.body().child("w:tbl");
    auto tbl_pr = tbl.child("w:tblPr");
    EXPECT_EQ(docx::val(tbl_pr, "w:tblStyle"), "Grid");
    EXPECT_EQ(tbl_pr.child("w:tblW").attribute("w:w").as_int(), 4000);
    EXPECT_STREQ(tbl_pr.child("w:tblW").attribute("w:type").as_string(), "pct");
    EXPECT_EQ(docx::val(tbl_pr, "w:jc"), "center");
    auto col = tbl.child("w:tblGrid").child("w:gridCol");
    EXPECT_NEAR(col.attribute("w:w").as_int(), 3 * col.next_sibling().attribute("w:w").as_int(), 3);

    // The header and direct formatting are on the cells.
    auto header = tbl.child("w:tr").child("w:tc").child("w:tcPr");
    EXPECT_STREQ(header.child("w:shd").attribute("w:fill").as_string(), "dddddd");
    EXPECT_EQ(header.child("w:tcBorders").child("w:bottom").attribute("w:sz").as_int(), 12);
    EXPECT_FALSE(header.child("w:tcBorders").child("w:top")) << "the table style has it";
    auto body = tbl.child("w:tr").next_sibling("w:tr").child("w:tc");
    EXPECT_EQ(
        body.child("w:tcPr").child("w:tcBorders").child("w:top").attribute("w:sz").as_int(), 12)
        << "the line below the header, on both sides of it";
    EXPECT_STREQ(
        body.next_sibling("w:tc").child("w:tcPr").child("w:shd").attribute("w:fill").as_string(),
        "ffcccc");
}
