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

// The private pieces under the DOCX format: namespace prefixes, the OPC
// package and image sizes.

#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "bytes.h"
#include "docx/ooxml.h"
#include "image_size.h"
#include "opc/package.h"

using namespace docsmith;

TEST(XML_Namespaces, PrefixesFollowTheUri)
{
    // Any prefix, a default namespace, and a declaration on an inner element.
    const std::string in =
        R"(<doc:document xmlns:doc="http://schemas.openxmlformats.org/wordprocessingml/2006/main" )"
        R"(xmlns:x="urn:example" xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" mc:Ignorable="x">)"
        R"(<doc:body><p xmlns="http://schemas.openxmlformats.org/wordprocessingml/2006/main" val="1">)"
        R"(<r xmlns:rel="http://schemas.openxmlformats.org/officeDocument/2006/relationships" rel:id="rId1"/>)"
        R"(<x:thing/></p></doc:body></doc:document>)";
    pugi::xml_document xml;
    ASSERT_TRUE(docx::load(xml, in));
    auto p = xml.child("w:document").child("w:body").child("w:p");
    ASSERT_TRUE(p);
    EXPECT_STREQ(p.attribute("val").as_string(), "1") << "unprefixed attributes have no namespace";
    EXPECT_STREQ(p.child("w:r").attribute("r:id").as_string(), "rId1");
    EXPECT_TRUE(p.child("x:thing")) << "an unknown namespace keeps its prefix";
    EXPECT_FALSE(p.attribute("xmlns"));

    auto root = xml.child("w:document");
    EXPECT_STREQ(root.attribute("xmlns:w").as_string(), docx::uri_of("w"));
    EXPECT_STREQ(root.attribute("xmlns:r").as_string(), docx::uri_of("r"));
    EXPECT_STREQ(root.attribute("xmlns:x").as_string(), "urn:example");
    EXPECT_STREQ(root.attribute("mc:Ignorable").as_string(), "x");
}

TEST(XML_Namespaces, ClashingPrefixIsRenamed)
{
    // "w" bound to something else: it can't keep "w", which WordprocessingML has.
    const std::string in =
        R"(<a:root xmlns:a="http://schemas.openxmlformats.org/wordprocessingml/2006/main" )"
        R"(xmlns:w="urn:other" xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" )"
        R"(mc:Ignorable="w"><w:x/></a:root>)";
    pugi::xml_document xml;
    ASSERT_TRUE(docx::load(xml, in));
    auto root = xml.child("w:root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root.child("ns1:x"));
    EXPECT_STREQ(root.attribute("xmlns:ns1").as_string(), "urn:other");
    EXPECT_STREQ(root.attribute("mc:Ignorable").as_string(), "ns1");
}

TEST(OPC_Package, Paths)
{
    using opc::package;
    EXPECT_EQ(package::resolve("word/document.xml", "media/a.png"), "word/media/a.png");
    EXPECT_EQ(
        package::resolve("word/document.xml", "../customXml/item1.xml"), "customXml/item1.xml");
    EXPECT_EQ(package::resolve("", "word/document.xml"), "word/document.xml");
    EXPECT_EQ(package::resolve("word/document.xml", "/word/styles.xml"), "word/styles.xml");
    EXPECT_EQ(package::relative("word/document.xml", "word/media/a.png"), "media/a.png");
    EXPECT_EQ(package::relative("word/document.xml", "docProps/core.xml"), "../docProps/core.xml");
    EXPECT_EQ(package::relative("", "word/document.xml"), "word/document.xml");
}

TEST(OPC_Package, RoundTrip)
{
    opc::package pkg;
    pkg.set("word/document.xml", "<w:document/>", docx::content_type::document);
    pkg.set("word/media/a.png", "PNG", "image/png");
    pkg.set("customXml/item1.xml", "<x/>", "application/xml");
    EXPECT_EQ(pkg.ensure_rel("", docx::rel::office_document, "word/document.xml"), "rId1");
    const auto img = pkg.ensure_rel("word/document.xml", docx::rel::image, "media/a.png");
    EXPECT_EQ(pkg.ensure_rel("word/document.xml", docx::rel::image, "media/a.png"), img)
        << "reused";
    pkg.ensure_rel("word/document.xml", docx::rel::hyperlink, "https://example.com", true);

    const auto again = opc::package::read_bytes(pkg.write_bytes());
    EXPECT_EQ(again.find("word/document.xml")->content_type, docx::content_type::document);
    EXPECT_EQ(again.find("word/media/a.png")->content_type, "image/png");
    EXPECT_EQ(again.find("customXml/item1.xml")->content_type, "application/xml");
    ASSERT_NE(again.find_rels("word/document.xml"), nullptr);
    EXPECT_EQ(*again.find_rels("word/document.xml"), pkg.rels("word/document.xml"));
    EXPECT_EQ(again.find_rel_by_type("", docx::rel::office_document)->target, "word/document.xml");
    EXPECT_EQ(again.parts().size(), 3U) << "relationship parts aren't parts";
}

TEST(ImageSize, Png)
{
    auto s = detail::read_image_size(detail::read_bytes("odt/smiley.png"));
    ASSERT_TRUE(s);
    EXPECT_GT(s->width, 0U);
    EXPECT_GT(s->height, 0U);
}

TEST(ImageSize, PngDensity)
{
    // IHDR 2x3, then pHYs at 3780 pixels per metre (96 dpi) x 7559 (192 dpi).
    std::string png("\x89PNG\r\n\x1a\n", 8);
    auto chunk = [&](const char *type, std::string data)
    {
        const auto n = static_cast<std::uint32_t>(data.size());
        png += {char(n >> 24), char(n >> 16), char(n >> 8), char(n)};
        png += type + data + std::string(4, '\0');
    };
    chunk("IHDR", std::string("\0\0\0\x02\0\0\0\x03\x08\x02\0\0\0", 13));
    chunk("pHYs", std::string("\0\0\x0e\xc4\0\0\x1d\x87\x01", 9));
    chunk("IEND", "");
    auto s = detail::read_image_size(png);
    ASSERT_TRUE(s);
    EXPECT_EQ(s->width, 2U);
    EXPECT_EQ(s->height, 3U);
    EXPECT_NEAR(s->dpi_x, 96, 0.1);
    EXPECT_NEAR(s->dpi_y, 192, 0.1);
}

TEST(ImageSize, Jpeg)
{
    // SOI, APP0 JFIF at 300 dpi, SOF0 with height 20 and width 10.
    const std::string jpeg("\xff\xd8"
                           "\xff\xe0\x00\x10JFIF\x00\x01\x01\x01\x01\x2c\x01\x2c\x00\x00"
                           "\xff\xc0\x00\x0b\x08\x00\x14\x00\x0a\x01\x01\x11\x00",
        33);
    auto s = detail::read_image_size(jpeg);
    ASSERT_TRUE(s);
    EXPECT_EQ(s->width, 10U);
    EXPECT_EQ(s->height, 20U);
    EXPECT_EQ(s->dpi_x, 300);
}

TEST(ImageSize, Unknown) { EXPECT_FALSE(detail::read_image_size("not an image")); }
