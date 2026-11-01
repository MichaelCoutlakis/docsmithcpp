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

#include <gtest/gtest.h>
#include <pugixml.hpp>

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
    auto red = doc.add_style(style("Red", text_props(colour::rgb(255, 0, 0))));
    auto both = doc.add_style(style("Both", text_props(font_weight::bold)));
    doc.add(paragraph("Bold ", span("red").set_style(red), span("span").set_style(both))
            .set_style(both));
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
    style *h1 = f.doc.styles().find("Heading_20_1");
    ASSERT_NE(h1, nullptr);
    h1->m_text_props->set(font_size(20));
    list_style *l1 = f.doc.list_styles().find("L1");
    ASSERT_NE(l1, nullptr);
    std::get<list_style_num>(l1->m_level_styles.front()).m_start_from = 3;
    f.save("odt/out/moderate_edited_style.odt");

    const part styles("odt/out/moderate_edited_style.odt", "styles.xml");
    EXPECT_EQ((definitions(styles_of(styles)).at({"Heading_20_1", "paragraph"})), 1);
    const part content("odt/out/moderate_edited_style.odt", "content.xml");
    EXPECT_EQ((definitions(automatic_styles_of(content)).at({"L1", "list"})), 1);

    const auto again = odt::file::load("odt/out/moderate_edited_style.odt");
    EXPECT_EQ(*again.doc.styles().find("Heading_20_1"), *h1);
    EXPECT_EQ(*again.doc.list_styles().find("L1"), *l1);
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
    auto note = doc.add_style(style("Note", text_props(font_style::italic)));
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
    text_doc doc{paragraph(field{field_type::page_number}, field{field_type::page_count},
        field{field_type::title}, field{field_type::date})};
    doc.meta.title = "Report";
    odt::file(doc).save("odt/out/field_elements.odt");

    part content("odt/out/field_elements.odt", "content.xml");
    auto p = content.xml.select_node("//text:p").node();
    EXPECT_STREQ(p.child("text:page-number").attribute("text:select-page").as_string(), "current");
    EXPECT_STREQ(p.child("text:page-count").text().as_string(), "1");
    EXPECT_STREQ(p.child("text:title").text().as_string(), "Report") << "the value it shows";
    EXPECT_EQ(std::string(p.child("text:date").attribute("text:date-value").as_string()).size(), 10U);
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
                      .find_child_by_attribute("style:page-layout", "style:name",
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
    EXPECT_STREQ(masters.select_node("//style:footer/text:p").node().text().as_string(), "New bottom");
}

TEST(ODT_XML, TableOfContentsEntries)
{
    const text_doc doc{table_of_contents("Contents", 2), heading(1, "Intro"),
        heading(2, span(bookmark{"results"}, "Results")), heading(3, "Too deep")};
    odt::file(doc).save("odt/out/toc_entries.odt");

    part content("odt/out/toc_entries.odt", "content.xml");
    auto toc = content.xml.select_node("//text:table-of-content").node();
    ASSERT_TRUE(toc);
    EXPECT_EQ(toc.child("text:table-of-content-source").attribute("text:outline-level").as_int(), 2);
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
