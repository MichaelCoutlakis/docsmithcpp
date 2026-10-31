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

#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "docsmithcpp/html/file.h"
#include "docsmithcpp/odt/file.h"

using namespace docsmith;

namespace
{
bool has(const std::string &s, const std::string &needle) { return s.find(needle) != std::string::npos; }

size_t count(const std::string &s, const std::string &needle)
{
    size_t n = 0;
    for(auto pos = s.find(needle); pos != std::string::npos; pos = s.find(needle, pos + 1))
        ++n;
    return n;
}

std::string read(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
} // namespace

TEST(HTML, BasicStructure)
{
    const auto html = html::file(text_doc{heading(1, "Title"), paragraph("Body text.")}).to_string();

    EXPECT_TRUE(html.starts_with("<!DOCTYPE html>"));
    EXPECT_TRUE(has(html, "<title>Title</title>")) << "defaults to the first heading";
    EXPECT_TRUE(has(html, "<h1>Title</h1>"));
    EXPECT_TRUE(has(html, "<p>Body text.</p>"));
}

TEST(HTML, EscapesTextAndAttributes)
{
    html::file page(text_doc{
        paragraph("a < b & \"c\" ", hyperlink("https://x.org/?a=1&b=\"2\"", "link"))});
    page.title = "T&C";
    const auto html = page.to_string();

    EXPECT_TRUE(has(html, "a &lt; b &amp; \"c\" "));
    EXPECT_TRUE(has(html, R"(<a href="https://x.org/?a=1&amp;b=&quot;2&quot;">link</a>)"));
    EXPECT_TRUE(has(html, "<title>T&amp;C</title>"));
}

TEST(HTML, EmptyElementsAreNotSelfClosed)
{
    // To an HTML parser <p/> and <a id="x"/> are open tags that swallow what follows.
    const auto html = html::file(text_doc{paragraph(), paragraph(bookmark{"mark"})}).to_string();

    EXPECT_TRUE(has(html, "<p></p>"));
    EXPECT_TRUE(has(html, R"(<a id="mark"></a>)"));
    EXPECT_FALSE(has(html, "/>"));
}

TEST(HTML, StylesBecomeClassesAndCss)
{
    style big("Big Text", text_props(font_size(18), font_style::italic));
    text_doc doc{paragraph("x").set_style(big), paragraph(span("y").set_style(big))};
    doc.styles().add(big);

    html::file page(doc);
    page.css = "body { margin: 0; }";
    const auto html = page.to_string();

    EXPECT_TRUE(has(html, ".Big_Text { font-size: 18pt; font-style: italic; }"));
    EXPECT_TRUE(has(html, "body { margin: 0; }"));
    EXPECT_TRUE(has(html, R"(<p class="Big_Text">x</p>)"));
    EXPECT_TRUE(has(html, R"(<p><span class="Big_Text">y</span></p>)"));
}

TEST(HTML, ListsFollowListStyles)
{
    list_style alpha("L1", {list_style_num(style{}, 1, list_enum::lower_alpha, ")", 3)});
    list_style bullets("L2", {list_style_bullet(style{}, 1, bullet_type::bullet())});
    text_doc doc{
        list(list_item("a"), list_item("b")).set_style(alpha),
        list(list_item("x")).set_style(bullets),
        list(list_item("plain")),
    };
    doc.list_styles().add(alpha);
    doc.list_styles().add(bullets);
    const auto html = html::file(doc).to_string();

    EXPECT_TRUE(has(html, R"(<ol class="L1" type="a" start="3">)"));
    EXPECT_TRUE(has(html, R"(<ul class="L2">)"));
    EXPECT_TRUE(has(html, "<ul>")) << "an unstyled list is bulleted";
    EXPECT_TRUE(has(html, "<li>a</li>")) << "one plain paragraph is written tight";
}

TEST(HTML, NestedListContinuesOuterStyle)
{
    list_style ls("L1", {list_style_bullet(style{}, 1, bullet_type::bullet()),
                            list_style_num(style{}, 2, list_enum::upper_roman)});
    text_doc doc{list(list_item(paragraph("outer"), list(list_item("inner")))).set_style(ls)};
    doc.list_styles().add(ls);
    const auto html = html::file(doc).to_string();

    EXPECT_TRUE(has(html, R"(<ul class="L1">)"));
    EXPECT_TRUE(has(html, R"(<ol type="I">)")) << "level 2 of L1 is numbered";
}

TEST(HTML, TableSpansAndCoveredCells)
{
    table t = table::from_cells({
        {table_cell(paragraph("wide")).set_span(1, 2), {}},
        {table_cell(paragraph("a")), table_cell(paragraph("b"))},
    });
    const auto html = html::file(text_doc{t}).to_string();

    EXPECT_TRUE(has(html, R"(<td colspan="2">wide</td>)"));
    EXPECT_EQ(count(html, "<td"), 3U) << "the covered cell isn't written";
}

TEST(HTML, InternalLinksMatchBookmarkIds)
{
    text_doc doc{paragraph("target", bookmark{"Bookmark 1"}),
        paragraph(hyperlink("#Bookmark 1", "back"))};
    const auto html = html::file(doc).to_string();

    EXPECT_TRUE(has(html, R"(<a id="Bookmark_1"></a>)"));
    EXPECT_TRUE(has(html, R"(<a href="#Bookmark_1">back</a>)"));
}

TEST(HTML, UnknownContentIsLeftOut)
{
    text_doc doc;
    doc.body.push_back(block_node{unknown{"<text:sequence-decls/>"}});
    doc.add(paragraph("kept").add(unknown{"<text:custom/>"}));
    const auto html = html::file(doc).to_string();

    EXPECT_TRUE(has(html, "<p>kept</p>"));
    EXPECT_FALSE(has(html, "text:"));
}

TEST(HTML, ImagePathsAreRelativeToThePage)
{
    // The test runs in tests/; the page goes to tests/html/out/.
    text_doc doc{paragraph(frame(image{"odt/smiley.png"}))};
    EXPECT_TRUE(has(html::file(doc).to_string(), R"(src="odt/smiley.png")"));

    html::file(doc).save("html/out/image.html");
    EXPECT_TRUE(has(read("html/out/image.html"), R"(<img src="../../odt/smiley.png" alt="">)"));
}

TEST(HTML, ConvertsParsedOdt)
{
    html::file(odt::file::load("odt/moderate.odt").doc).save("html/out/moderate.html");
    const auto html = read("html/out/moderate.html");

    EXPECT_TRUE(has(html, "Heading 1</h1>"));
    EXPECT_TRUE(has(html, R"(<a href="https://github.com/MichaelCoutlakis/docsmithcpp">docsmith</a>)"));
    EXPECT_EQ(count(html, "<li"), 8U); // 2 + 2 + 1 + (1 with a nested 1) + 1
}
