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

// White-box tests: what the ODT writer puts in content.xml and styles.xml,
// which reading the file back through odt::file can't show (e.g. a definition
// written twice).

#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "docsmithcpp/html/file.h"
#include "docsmithcpp/odt/file.h"
#include "odf/package.h"

using namespace docsmith;

namespace
{
/// One XML part of a saved package.
struct part
{
    part(const std::string &path, const std::string &name)
    {
        pkg = odf::package::read(path);
        const auto *e = pkg.find(name);
        EXPECT_NE(e, nullptr) << name;
        if(e)
            xml.load_buffer(e->data.data(), e->data.size());
    }
    odf::package pkg;
    pugi::xml_document xml;
};

pugi::xml_node styles_of(const part &styles_xml)
{ return styles_xml.xml.child("office:document-styles").child("office:styles"); }

pugi::xml_node automatic_styles_of(const part &content_xml)
{ return content_xml.xml.child("office:document-content").child("office:automatic-styles"); }

/// How often each (name, family) is defined in a style container. List styles
/// count under the family "list".
std::map<std::pair<std::string, std::string>, int> definitions(const pugi::xml_node &container)
{
    std::map<std::pair<std::string, std::string>, int> out;
    for(auto c : container.children())
    {
        const std::string tag = c.name();
        if(tag == "style:style")
            ++out[{c.attribute("style:name").as_string(), c.attribute("style:family").as_string()}];
        else if(tag == "text:list-style")
            ++out[{c.attribute("style:name").as_string(), "list"}];
    }
    return out;
}
} // namespace

TEST(ODT_XML, StylesGoInStylesXml)
{
    text_doc doc;
    auto red = doc.styles()
                   .insert(text_style("Red", text_props(font_colour(colour::rgb(255, 0, 0)))))
                   .first;
    auto both = doc.styles().insert(text_style("Both", text_props(font_weight::bold))).first;
    auto both_para =
        doc.styles().insert(paragraph_style("Both", text_props(font_weight::bold))).first;
    doc.add(paragraph("Bold ", span("red").set_style(red), span("span").set_style(both))
            .set_style(both_para));
    odt::file(doc).save("odt/out/styles_in_styles_xml.odt");

    const part styles("odt/out/styles_in_styles_xml.odt", "styles.xml");
    const auto defs = definitions(styles_of(styles));
    EXPECT_EQ((defs.at({"Red", "text"})), 1);
    EXPECT_EQ((defs.at({"Both", "text"})), 1);
    EXPECT_EQ((defs.at({"Both", "paragraph"})), 1);

    const part content("odt/out/styles_in_styles_xml.odt", "content.xml");
    EXPECT_FALSE(content.xml.child("office:document-content").child("office:styles"))
        << "content.xml has no common styles in ODF";
}

TEST(ODT_XML, ResaveWritesEachDefinitionOnce)
{
    const auto f = odt::file::load("odt/moderate.odt");
    ASSERT_FALSE(f.doc.styles().empty());
    f.save("odt/out/moderate_once.odt");

    const part styles("odt/out/moderate_once.odt", "styles.xml");
    const part content("odt/out/moderate_once.odt", "content.xml");
    for(const auto &container : {styles_of(styles), automatic_styles_of(content)})
        for(const auto &[def, n] : definitions(container))
            EXPECT_EQ(n, 1) << def.first << " (" << def.second << ")";

    // Unchanged definitions keep the XML the model doesn't hold.
    auto h1 =
        styles_of(styles).find_child_by_attribute("style:style", "style:name", "Heading_20_1");
    EXPECT_STREQ(h1.attribute("style:display-name").as_string(), "Heading 1");
}

TEST(ODT_XML, EditedStyleReplacesTheOriginal)
{
    auto f = odt::file::load("odt/moderate.odt");
    paragraph_style *h1 = f.doc.styles().find(builtin::heading(1));
    ASSERT_NE(h1, nullptr);
    h1->text.set(font_size(20));
    list_style *l1 = f.doc.styles().list.find("L1");
    ASSERT_NE(l1, nullptr);
    std::get<number_level>(l1->levels.front()).start_from = 3;
    f.save("odt/out/moderate_edited_style.odt");

    const part styles("odt/out/moderate_edited_style.odt", "styles.xml");
    EXPECT_EQ((definitions(styles_of(styles)).at({"Heading_20_1", "paragraph"})), 1);
    const part content("odt/out/moderate_edited_style.odt", "content.xml");
    EXPECT_EQ((definitions(automatic_styles_of(content)).at({"L1", "list"})), 1);

    const auto again = odt::file::load("odt/out/moderate_edited_style.odt");
    EXPECT_EQ(*again.doc.styles().find(builtin::heading(1)), *h1);
    EXPECT_EQ(*again.doc.styles().list.find("L1"), *l1);
}

TEST(ODT_XML, LoadedStylesXmlIsKept)
{
    // Page layouts and master pages (headers, footers) aren't modelled; saving
    // keeps them.
    odt::file::load("odt/moderate.odt").save("odt/out/moderate_master.odt");
    const part styles("odt/out/moderate_master.odt", "styles.xml");
    EXPECT_TRUE(styles.xml.child("office:document-styles")
            .child("office:master-styles")
            .child("style:master-page"));
}

TEST(ODT_XML, HeaderRowsAreGrouped)
{
    table t = table::from_cells({{{"Name"}, {"Value"}}, {{"a"}, {"1"}}}).set_header_rows(1);
    odt::file(text_doc{t}).save("odt/out/header_rows_xml.odt");

    const part content("odt/out/header_rows_xml.odt", "content.xml");
    auto xml_table = content.xml.select_node("//table:table").node();
    auto header = xml_table.child("table:table-header-rows");
    ASSERT_TRUE(header);
    EXPECT_EQ(header.select_nodes("table:table-row").size(), 1U);
    EXPECT_EQ(xml_table.select_nodes("table:table-row").size(), 1U);
    EXPECT_EQ(header.previous_sibling().name(), std::string("table:table-column"));
}

TEST(ODT_XML, WhiteSpaceElements)
{
    odt::file(text_doc{paragraph(" a  b\tc\nd")}).save("odt/out/white_space_xml.odt");
    const part content("odt/out/white_space_xml.odt", "content.xml");
    std::ostringstream p;
    content.xml.select_node("//text:p").node().print(p, "", pugi::format_raw);
    EXPECT_EQ(p.str(), "<text:p><text:s/>a <text:s/>b<text:tab/>c<text:line-break/>d</text:p>");
}

TEST(ODT_XML, ReadsWhiteSpaceAsOdfDoes)
{
    // As LibreOffice writes it: white space in text collapses, and is dropped
    // at the start of a paragraph.
    odf::package pkg("application/vnd.oasis.opendocument.text");
    pkg.set("content.xml",
        R"(<?xml version="1.0" encoding="UTF-8"?>
<office:document-content xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" office:version="1.2">
  <office:body>
    <office:text>
      <text:p>  a
        b <text:span>c</text:span> <text:span>d</text:span><text:s text:c="3"/>e<text:tab/>f<text:line-break/>g</text:p>
    </office:text>
  </office:body>
</office:document-content>)",
        "text/xml");
    pkg.write("odt/out/white_space_read.odt");

    const text_doc expected{paragraph("a b ", span("c"), " ", span("d"), "   e\tf\ng")};
    EXPECT_EQ(odt::file::load("odt/out/white_space_read.odt").doc.body, expected.body);
}

TEST(ODT_XML, PageBreakFoldsIntoNextParagraph)
{
    text_doc doc;
    auto note = doc.styles().insert(paragraph_style("Note", text_props(font_style::italic))).first;
    doc.add(paragraph("one")).add(page_break{}).add(paragraph("two").set_style(note));
    odt::file(doc).save("odt/out/page_break_xml.odt");

    const part content("odt/out/page_break_xml.odt", "content.xml");
    const auto paragraphs = content.xml.select_nodes("//text:p");
    ASSERT_EQ(paragraphs.size(), 2U) << "no extra paragraph";
    EXPECT_STREQ(paragraphs[1].node().attribute("text:style-name").as_string(), "Note.PageBreak");

    auto s = automatic_styles_of(content).find_child_by_attribute(
        "style:style", "style:name", "Note.PageBreak");
    EXPECT_STREQ(s.attribute("style:parent-style-name").as_string(), "Note");
    EXPECT_STREQ(
        s.child("style:paragraph-properties").attribute("fo:break-before").as_string(), "page");
}

TEST(ODT_XML, BuiltinStylesDefinedOnce)
{
    const text_doc doc{paragraph("log\nlines").set_style(builtin::preformatted),
        paragraph(span("code").set_style(builtin::source_text))};
    odt::file(doc).save("odt/out/builtin_styles.odt");
    odt::file::load("odt/out/builtin_styles.odt").save("odt/out/builtin_styles_resaved.odt");

    for(const char *file : {"odt/out/builtin_styles.odt", "odt/out/builtin_styles_resaved.odt"})
    {
        const auto defs = definitions(styles_of(part(file, "styles.xml")));
        EXPECT_EQ((defs.at({"Preformatted_20_Text", "paragraph"})), 1) << file;
        EXPECT_EQ((defs.at({"Source_20_Text", "text"})), 1) << file;
        EXPECT_FALSE(defs.contains({"Title", "paragraph"})) << "only the builtins used";
    }

    // moderate.odt defines Source Text already.
    auto f = odt::file::load("odt/moderate.odt");
    f.doc.add(paragraph(span("x").set_style(builtin::source_text)));
    f.save("odt/out/moderate_source_text.odt");
    EXPECT_EQ((definitions(styles_of(part("odt/out/moderate_source_text.odt", "styles.xml")))
                      .at({"Source_20_Text", "text"})),
        1);
}

TEST(ODT_XML, FieldElements)
{
    text_doc doc{paragraph(field{field_type::page_number},
        field{field_type::page_count},
        field{field_type::title},
        field{field_type::date})};
    doc.meta.title = "Report";
    odt::file(doc).save("odt/out/field_elements.odt");

    part content("odt/out/field_elements.odt", "content.xml");
    auto p = content.xml.select_node("//text:p").node();
    EXPECT_STREQ(p.child("text:page-number").attribute("text:select-page").as_string(), "current");
    EXPECT_STREQ(p.child("text:page-count").text().as_string(), "1");
    EXPECT_STREQ(p.child("text:title").text().as_string(), "Report") << "the value it shows";
    EXPECT_EQ(
        std::string(p.child("text:date").attribute("text:date-value").as_string()).size(), 10U);
}

TEST(ODT_XML, HeaderAndFooterGoOnTheMasterPage)
{
    text_doc doc{paragraph("Body")};
    doc.page.set_header(paragraph("Top"));
    doc.page.set_footer(paragraph("Bottom"));
    odt::file(doc).save("odt/out/master_page.odt");

    part styles("odt/out/master_page.odt", "styles.xml");
    auto root = styles.xml.child("office:document-styles");
    auto master = root.child("office:master-styles").child("style:master-page");
    EXPECT_STREQ(master.attribute("style:name").as_string(), "Standard");
    EXPECT_STREQ(master.first_child().name(), "style:header");
    EXPECT_STREQ(master.child("style:footer").child("text:p").text().as_string(), "Bottom");
    auto layout = root.child("office:automatic-styles")
                      .find_child_by_attribute("style:page-layout",
                          "style:name",
                          master.attribute("style:page-layout-name").as_string());
    ASSERT_TRUE(layout);
    EXPECT_TRUE(layout.child("style:header-style").child("style:header-footer-properties"));

    // Saved again: still one master page, the footer in place.
    auto f = odt::file::load("odt/out/master_page.odt");
    f.doc.page.set_footer(paragraph("New bottom"));
    f.save("odt/out/master_page_resaved.odt");
    part again("odt/out/master_page_resaved.odt", "styles.xml");
    auto masters = again.xml.child("office:document-styles").child("office:master-styles");
    EXPECT_EQ(masters.select_nodes("style:master-page").size(), 1U);
    EXPECT_STREQ(masters.child("style:master-page").last_child().name(), "style:footer");
    EXPECT_STREQ(
        masters.select_node("//style:footer/text:p").node().text().as_string(), "New bottom");
}

TEST(ODT_XML, TableOfContentsEntries)
{
    const text_doc doc{table_of_contents("Contents", 2),
        heading(1, "Intro"),
        heading(2, span(bookmark{"results"}, "Results")),
        heading(3, "Too deep")};
    odt::file(doc).save("odt/out/toc_entries.odt");

    part content("odt/out/toc_entries.odt", "content.xml");
    auto toc = content.xml.select_node("//text:table-of-content").node();
    ASSERT_TRUE(toc);
    EXPECT_EQ(
        toc.child("text:table-of-content-source").attribute("text:outline-level").as_int(), 2);
    auto body = toc.child("text:index-body");
    EXPECT_STREQ(body.child("text:index-title").child("text:p").text().as_string(), "Contents");

    std::vector<std::pair<std::string, std::string>> entries;
    for(auto p : body.children("text:p"))
        entries.emplace_back(p.attribute("text:style-name").as_string(),
            p.child("text:a").attribute("xlink:href").as_string());
    const std::vector<std::pair<std::string, std::string>> expected{
        {"Contents_20_1", "#Intro|outline"}, {"Contents_20_2", "#results"}};
    EXPECT_EQ(entries, expected) << "a bookmark in the heading is its link target";

    part styles("odt/out/toc_entries.odt", "styles.xml");
    auto defs = definitions(styles_of(styles));
    EXPECT_EQ((defs[{"Contents_20_Heading", "paragraph"}]), 1);
    EXPECT_EQ((defs[{"Contents_20_2", "paragraph"}]), 1);
}

TEST(ODT_XML, FontsAreDeclared)
{
    text_doc doc;
    doc.add(paragraph("x").set_style(doc.styles()
            .insert(paragraph_style("Body", text_props(font_name("Liberation Serif"))))
            .first));
    odt::file(doc).save("odt/out/font_faces.odt");

    for(const char *name : {"styles.xml", "content.xml"})
    {
        part p("odt/out/font_faces.odt", name);
        auto face = p.xml.select_node("//office:font-face-decls/style:font-face").node();
        EXPECT_STREQ(face.attribute("style:name").as_string(), "Liberation Serif") << name;
        EXPECT_STREQ(face.attribute("svg:font-family").as_string(), "'Liberation Serif'") << name;
    }
}

TEST(ODT_XML, DisplayNamesForEncodedStyleNames)
{
    text_doc doc;
    doc.add(paragraph("x").set_style(
        doc.styles().insert(paragraph_style("Text body", text_props(font_size(11)))).first));
    doc.add(paragraph("y").set_style(
        doc.styles().insert(paragraph_style("Plain", text_props(font_size(11)))).first));
    odt::file(doc).save("odt/out/display_names.odt");

    part styles("odt/out/display_names.odt", "styles.xml");
    auto body =
        styles_of(styles).find_child_by_attribute("style:style", "style:name", "Text_20_body");
    EXPECT_STREQ(body.attribute("style:display-name").as_string(), "Text body")
        << "so LibreOffice takes it as its own Text Body";
    auto plain = styles_of(styles).find_child_by_attribute("style:style", "style:name", "Plain");
    EXPECT_FALSE(plain.attribute("style:display-name"));
}

TEST(ODT_XML, ListNumbering)
{
    list l(list_item("header"), list_item("one"), list_item("more"), list_item("five"));
    l.items()[0].numbered = false;
    l.items()[2].numbered = false;
    l.items()[3].start_value = 5;
    list more(list_item("six"));
    more.continue_numbering = true;
    odt::file(text_doc{l, paragraph("between"), more}).save("odt/out/list_numbering.odt");

    const part content("odt/out/list_numbering.odt", "content.xml");
    auto lists = content.xml.select_nodes("//office:text/text:list");
    ASSERT_EQ(lists.size(), 2U);
    auto first = lists[0].node();
    EXPECT_STREQ(first.first_child().name(), "text:list-header");
    // Only the first item can be unnumbered: a later one is more of the one
    // before it, as LibreOffice saves one.
    EXPECT_EQ(first.select_nodes("text:list-item").size(), 2U);
    EXPECT_EQ(first.select_nodes("text:list-item[1]/text:p").size(), 2U);
    EXPECT_EQ(
        first.select_node("text:list-item[2]").node().attribute("text:start-value").as_int(), 5);
    EXPECT_TRUE(lists[1].node().attribute("text:continue-numbering").as_bool());
}

// Each level of a list style is indented further than the one above, as in
// DOCX: without the level's properties LibreOffice starts every level at the
// margin.
TEST(ODT_XML, ListLevelIndent)
{
    text_doc doc{list(list_item("one", list(list_item("two")))).set_style("Steps")};
    doc.styles().insert(
        list_style("Steps", {bullet_level{.level = 1}, number_level{.level = 2}}));
    odt::file(doc).save("odt/out/list_level_indent.odt");

    const part content("odt/out/list_level_indent.odt", "content.xml");
    const auto levels = content.xml.select_nodes(
        "//text:list-style[@style:name='Steps']/*/style:list-level-properties");
    ASSERT_EQ(levels.size(), 2U);
    std::vector<double> margins;
    for(const auto &l : levels)
    {
        const auto props = l.node();
        EXPECT_STREQ(props.attribute("text:list-level-position-and-space-mode").as_string(),
            "label-alignment");
        const auto align = props.child("style:list-level-label-alignment");
        const std::string margin = align.attribute("fo:margin-left").as_string();
        EXPECT_EQ(margin, align.attribute("text:list-tab-stop-position").as_string());
        EXPECT_LT(align.attribute("fo:text-indent").as_double(), 0); // a hanging label
        margins.push_back(std::stod(margin));
    }
    EXPECT_GT(margins[0], 0);
    EXPECT_GT(margins[1], margins[0]);
}

// A column's cell style is its default cell style, and its paragraph style
// goes on the paragraphs of the column's cells.
TEST(ODT_XML, ColumnStyle)
{
    text_doc doc;
    doc.styles().insert(cell_style("Key", cell_props(para_style_name("Key"))));
    table t = table::from_cells({{"a", "1"}, {"b", "2"}});
    t.set_column_style(0, "Key");
    doc.add(t);
    odt::file(doc).save("odt/out/column_style_xml.odt");

    const part content("odt/out/column_style_xml.odt", "content.xml");
    const auto cols = content.xml.select_nodes("//table:table-column");
    ASSERT_FALSE(cols.empty());
    EXPECT_STREQ(cols[0].node().attribute("table:default-cell-style-name").as_string(), "Key");
    const auto cells = content.xml.select_nodes("//table:table-cell");
    ASSERT_EQ(cells.size(), 4U);
    for(size_t i : {0, 2})
    {
        EXPECT_STREQ(cells[i].node().attribute("table:style-name").as_string(), "Key");
        EXPECT_STREQ(cells[i].node().child("text:p").attribute("text:style-name").as_string(), "Key");
    }
    EXPECT_FALSE(cells[1].node().child("text:p").attribute("text:style-name"));
}

// style:font-name names a font declaration, not the family: LibreOffice
// declares "Liberation Sans1" for a second "Liberation Sans". The model's
// font_name is the family, so HTML gets a real font.
TEST(ODT_XML, FontNameIsTheFamily)
{
    text_doc doc;
    doc.styles().insert(paragraph_style("Sans", text_props(font_name("Liberation Sans"))));
    doc.add(paragraph("x").set_style("Sans"));
    odt::file(doc).save("odt/out/font_family.odt");

    // Rename the declaration as LibreOffice might.
    auto pkg = odf::package::read("odt/out/font_family.odt");
    const auto *e = pkg.find("styles.xml");
    pugi::xml_document xml;
    xml.load_buffer(e->data.data(), e->data.size());
    auto face = xml.select_node("//style:font-face[@style:name='Liberation Sans']").node();
    ASSERT_TRUE(face);
    face.attribute("style:name").set_value("Liberation Sans1");
    face.attribute("svg:font-family").set_value("'Liberation Sans', sans-serif");
    xml.select_node("//style:style[@style:name='Sans']/style:text-properties")
        .node()
        .attribute("style:font-name")
        .set_value("Liberation Sans1");
    std::ostringstream ss;
    xml.save(ss, "", pugi::format_raw);
    pkg.set("styles.xml", ss.str(), "text/xml");
    pkg.write("odt/out/font_family_renamed.odt");

    const auto f = odt::file::load("odt/out/font_family_renamed.odt");
    EXPECT_EQ(f.doc.styles().para.find("Sans")->text.m_font_name, font_name("Liberation Sans"));
    EXPECT_NE(html::file(f.doc).to_string().find(R"(font-family: "Liberation Sans";)"),
        std::string::npos);

    // Unchanged, the definition is written back as it was.
    f.save("odt/out/font_family_resaved.odt");
    const part styles("odt/out/font_family_resaved.odt", "styles.xml");
    EXPECT_STREQ(styles.xml.select_node("//style:style[@style:name='Sans']/style:text-properties")
                     .node()
                     .attribute("style:font-name")
                     .as_string(),
        "Liberation Sans1");
}

TEST(ODT_XML, FrameSize)
{
    text_doc doc;
    frame f(doc.add_image("Pictures/dot.png", "not really a png", "image/png"));
    f.width = pt(36);
    f.height = cm(1);
    doc.add(paragraph(f));
    odt::file(doc).save("odt/out/frame_size.odt");

    const part content("odt/out/frame_size.odt", "content.xml");
    auto n = content.xml.select_node("//draw:frame").node();
    EXPECT_STREQ(n.attribute("svg:width").as_string(), "36pt");
    EXPECT_STREQ(n.attribute("svg:height").as_string(), "28.346pt");
}

namespace
{
/// Every builtin default defined, and a node of each kind using it.
text_doc defaults_doc()
{
    text_doc doc;
    doc.styles().insert(paragraph_style(builtin::para,
        text_props(font_name("Liberation Serif"), font_size(11)),
        paragraph_props(space_after{pt(6)})));
    doc.styles().insert(paragraph_style(builtin::heading(1), text_props(font_weight::bold)));
    doc.styles().insert(text_style(builtin::link, text_props(font_colour(colour::rgb(0, 0, 200)))));
    doc.styles().insert(table_style(builtin::table, table_props(cell_padding{pt(2)})));
    doc.styles().insert(list_style(builtin::list,
        {number_level{.level = 1, .format = list_enum::arabic, .suffix = ".", .start_from = 1}}));
    list_item nested("one");
    nested.add(list(list_item("one.one")));
    doc.add(heading(1, "Introduction"))
        .add(paragraph("Hello ", hyperlink("https://example.com", "site")))
        .add(table::from_cells({{"a", "b"}}))
        .add(list(std::move(nested), list_item("two")));
    return doc;
}
} // namespace

// An unstyled node has its kind's default style. ODF writes an unstyled
// paragraph and link as such, and names the others' defaults.
TEST(ODT_XML, DefaultStyles)
{
    const text_doc doc = defaults_doc();
    odt::file(doc).save("odt/out/default_styles.odt");

    const part content("odt/out/default_styles.odt", "content.xml");
    EXPECT_STREQ(
        content.xml.select_node("//text:h").node().attribute("text:style-name").as_string(),
        "Heading_20_1");
    EXPECT_FALSE(
        content.xml.select_node("//office:text/text:p").node().attribute("text:style-name"));
    EXPECT_STREQ(
        content.xml.select_node("//table:table").node().attribute("table:style-name").as_string(),
        "Default_20_Table");
    const auto lists = content.xml.select_nodes("//text:list");
    ASSERT_EQ(lists.size(), 2U);
    EXPECT_STREQ(lists[0].node().attribute("text:style-name").as_string(), "Default_20_List");
    EXPECT_FALSE(lists[1].node().attribute("text:style-name"))
        << "a nested list continues the enclosing one's style";

    const part styles("odt/out/default_styles.odt", "styles.xml");
    for(const char *name : {"Standard", "Heading_20_1", "Internet_20_link", "Default_20_Table"})
        EXPECT_TRUE(styles.xml.select_node(
            (std::string("//office:styles/style:style[@style:name='") + name + "']").c_str()))
            << name;

    const auto back = odt::file::load("odt/out/default_styles.odt").doc;
    // The table's cells read back with the default table's formatting, as a
    // table style's does (DESIGN.md, Tables).
    ASSERT_EQ(back.body.size(), doc.body.size());
    for(size_t i : {0, 1, 3})
        EXPECT_EQ(back.body[i], doc.body[i]) << "the defaults read back as no style";
    const auto &t = std::get<table>(back.body[2].v);
    EXPECT_TRUE(t.style.empty());
    EXPECT_EQ(effective_cell_props(t, back.styles(), 0, 0).m_padding, pt(2));
    ASSERT_NE(back.styles().find(builtin::para), nullptr);
    ASSERT_NE(back.styles().find(builtin::heading(1)), nullptr);
    ASSERT_NE(back.styles().find(builtin::link), nullptr);
    ASSERT_NE(back.styles().find(builtin::table), nullptr);
    EXPECT_EQ(back.styles().find(builtin::para)->text, doc.styles().find(builtin::para)->text);
    EXPECT_NE(back.styles().find(builtin::list), nullptr);
}

// A template's defaults are what content added to it gets.
TEST(ODT_XML, LoadedDefaultStyles)
{
    auto f = odt::file::load("odt/moderate.odt");
    EXPECT_NE(f.doc.styles().find(builtin::para), nullptr);
    for(const paragraph *p : find_all<paragraph>(f.doc, [](const paragraph &) { return true; }))
        EXPECT_FALSE(p->style == builtin::para) << "read as no style";
}

// LibreOffice applies a style only if the file defines it, its own Heading 1
// too, so the writer defines the default heading and link styles it uses.
TEST(ODT_XML, BuiltinHeadingAndLinkStylesAreDefined)
{
    odt::file(text_doc{heading(2, "Results"), paragraph(hyperlink("https://example.com", "site"))})
        .save("odt/out/builtin_heading.odt");
    const part styles("odt/out/builtin_heading.odt", "styles.xml");
    auto h2 = styles.xml.select_node("//style:style[@style:name='Heading_20_2']").node();
    ASSERT_TRUE(h2);
    EXPECT_STREQ(h2.child("style:text-properties").attribute("fo:font-weight").as_string(), "bold");
    EXPECT_STREQ(h2.attribute("style:default-outline-level").as_string(), "2");
    EXPECT_TRUE(styles.xml.select_node("//style:style[@style:name='Internet_20_link']"));
    EXPECT_FALSE(styles.xml.select_node("//style:style[@style:name='Heading_20_1']"))
        << "only the levels used";
}

// A template's defaults are loaded as the builtin styles. Replacing one, or
// editing the one styles()[] gives, is what is saved, in place.
TEST(ODT_XML, TemplateDefaultStylesCanBeReplacedOrEdited)
{
    auto f = odt::file::load("odt/moderate.odt");
    paragraph_style *h1 = &f.doc.styles()[builtin::heading(1)];
    EXPECT_EQ(h1->parent, para_style_name("Heading")) << "as LibreOffice defines it";
    ASSERT_NE(f.doc.styles().find(builtin::para), nullptr);
    ASSERT_NE(f.doc.styles().find(builtin::link), nullptr);

    // Edited in place: the rest of its definition stays.
    h1->text.set(font_size(20));
    // Replaced.
    f.doc.styles().insert_or_assign(
        paragraph_style(builtin::para, text_props(font_name("Liberation Sans"), font_size(11))));
    f.doc.add(paragraph("added"));
    f.save("odt/out/template_defaults.odt");

    const part styles("odt/out/template_defaults.odt", "styles.xml");
    EXPECT_EQ(
        styles.xml.select_nodes("//office:styles/style:style[@style:name='Standard']").size(), 1U);
    EXPECT_EQ(
        styles.xml.select_nodes("//office:styles/style:style[@style:name='Heading_20_1']").size(),
        1U);

    const auto back = odt::file::load("odt/out/template_defaults.odt").doc;
    const paragraph_style *h1_back = back.styles().find(builtin::heading(1));
    ASSERT_NE(h1_back, nullptr);
    EXPECT_EQ(h1_back->parent, para_style_name("Heading"));
    EXPECT_EQ(h1_back->text.m_font_size, font_size(20));
    const paragraph_style *standard = back.styles().find(builtin::para);
    ASSERT_NE(standard, nullptr);
    EXPECT_EQ(standard->text, text_props(font_name("Liberation Sans"), font_size(11)));
    EXPECT_TRUE(std::get<paragraph>(back.body.back().v).style.empty());
}

TEST(ODT_XML, StyleNamesAreSpelledAsOdfNames)
{
    text_doc doc;
    const auto note =
        doc.styles().insert(paragraph_style("Note (draft) 2", text_props(font_size(9)))).first;
    doc.add(paragraph("x").set_style(note)).add(paragraph("y"));
    doc.styles()[builtin::para].text.set(font_size(11));
    odt::file(doc).save("odt/out/odf_names.odt");

    part styles("odt/out/odf_names.odt", "styles.xml");
    EXPECT_TRUE(styles_of(styles).find_child_by_attribute(
        "style:style", "style:name", "Note_20__28_draft_29__20_2"));
    EXPECT_TRUE(styles_of(styles).find_child_by_attribute("style:style", "style:name", "Standard"))
        << "the default paragraph style is LibreOffice's Standard";

    // And back to the model's names.
    const auto again = odt::file::load("odt/out/odf_names.odt").doc;
    EXPECT_TRUE(again.styles().para.contains(note));
    EXPECT_TRUE(again.styles().para.contains(builtin::para));
    EXPECT_EQ(again.body, doc.body);
}

// Editing a builtin that the file doesn't define keeps its default look.
TEST(ODT_XML, EditedBuiltinKeepsDefault)
{
    text_doc doc{heading(1, "Results"), paragraph("code").set_style(builtin::preformatted)};
    doc.styles()[builtin::heading(1)].text.set(font_colour(colour::rgb(200, 0, 0)));
    doc.styles()[builtin::preformatted].para.set(space_after(pt(6)));
    odt::file(doc).save("odt/out/edited_builtin.odt");

    const part styles("odt/out/edited_builtin.odt", "styles.xml");
    auto h1 = styles.xml.select_node("//style:style[@style:name='Heading_20_1']").node();
    ASSERT_TRUE(h1);
    EXPECT_STREQ(h1.child("style:text-properties").attribute("fo:font-weight").as_string(), "bold");
    EXPECT_STREQ(h1.child("style:text-properties").attribute("fo:color").as_string(), "#c80000");
    EXPECT_STREQ(h1.attribute("style:default-outline-level").as_string(), "1");
    auto pre = styles.xml.select_node("//style:style[@style:name='Preformatted_20_Text']").node();
    ASSERT_TRUE(pre);
    EXPECT_STREQ(
        pre.child("style:text-properties").attribute("style:font-name").as_string(), "Courier New");

    // What is saved reads back as the definition.
    auto back = odt::file::load("odt/out/edited_builtin.odt");
    EXPECT_EQ(
        *back.doc.styles().find(builtin::heading(1)), *doc.styles().find(builtin::heading(1)));
}

TEST(ODT_XML, UnderlineRoundTrips)
{
    text_doc doc{paragraph(span("u").set_style("U"), span("n").set_style("N"))};
    doc.styles().insert(text_style("U", text_props(underline::single)));
    doc.styles().insert(text_style("N", text_props(underline::none)));
    odt::file(doc).save("odt/out/underline.odt");
    auto back = odt::file::load("odt/out/underline.odt");
    EXPECT_EQ(back.doc.styles().text.find("U")->text.m_underline, underline::single);
    EXPECT_EQ(back.doc.styles().text.find("N")->text.m_underline, underline::none);
}
