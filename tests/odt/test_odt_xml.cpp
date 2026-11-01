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
