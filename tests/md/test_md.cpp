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

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "docsmithcpp/html/file.h"
#include "docsmithcpp/md/file.h"
#include "docsmithcpp/odt/file.h"

using namespace docsmith;
namespace fs = std::filesystem;

namespace
{
std::string md_of(const text_doc &doc) { return md::file(doc).to_string(); }

std::string read(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

text_doc with_styles()
{
    text_doc doc;
    doc.add_style(style("Bold", text_props(font_weight::bold)));
    doc.add_style(style("Italic", text_props(font_style::italic)));
    doc.add_style(style("BoldItalic", text_props(font_weight::bold, font_style::italic)));
    style child("Child");
    child.m_parent_style = "Bold";
    doc.add_style(child);
    doc.add_style(style("Red", text_props(colour::rgb(255, 0, 0))));
    return doc;
}

list_style numbered(std::string name, int start = 1)
{
    return list_style(name,
        {list_style_num(style{}, 1, list_enum::arabic, ".", start),
            list_style_bullet(style{}, 2, bullet_type::bullet())});
}
} // namespace

// ---- Output ----------------------------------------------------------------

TEST(Markdown, HeadingsAndParagraphs)
{
    EXPECT_EQ(
        md_of(text_doc{
            heading(1, "Title"), paragraph("Body text."), heading(3, "Sub"), heading(9, "Deep")}),
        "# Title\n\nBody text.\n\n### Sub\n\n###### Deep\n");
}

TEST(Markdown, EmptyDocumentAndParagraphs)
{
    EXPECT_EQ(md_of(text_doc{}), "");
    EXPECT_EQ(md_of(text_doc{paragraph(), paragraph("a"), paragraph()}), "a\n");
}

TEST(Markdown, TitleIsALevelOneHeading)
{
    text_doc doc;
    doc.set_title("Report");
    EXPECT_EQ(md_of(doc), "# Report\n");
}

TEST(Markdown, StylesBecomeEmphasisStrongOrNothing)
{
    text_doc doc = with_styles();
    doc.add(paragraph(span("b").set_style("Bold"),
        " ",
        span("i").set_style("Italic"),
        " ",
        span("bi").set_style("BoldItalic"),
        " ",
        span("c").set_style("Child"),
        " ",
        span("red").set_style("Red"),
        " ",
        span("plain")));
    EXPECT_EQ(md_of(doc), "**b** *i* ***bi*** **c** red plain\n");
}

TEST(Markdown, EmphasisNamesWithoutDefinitions)
{
    EXPECT_EQ(md_of(text_doc{paragraph(
                  span("a").set_style("Emphasis"), span("b").set_style("Strong_20_Emphasis"))}),
        "*a***b**\n");
}

TEST(Markdown, SpacesStayOutsideEmphasis)
{
    text_doc doc = with_styles();
    doc.add(paragraph("a", span(" b ").set_style("Bold"), "c", span("  ").set_style("Bold")));
    EXPECT_EQ(md_of(doc), "a **b** c  \n");
}

TEST(Markdown, CodeSpans)
{
    EXPECT_EQ(md_of(text_doc{paragraph("Run ",
                  span("make all").set_style(builtin::source_text),
                  " or ",
                  span("a`b").set_style(builtin::source_text),
                  " or ",
                  span("`x").set_style(builtin::source_text))}),
        "Run `make all` or ``a`b`` or `` `x ``\n");
}

TEST(Markdown, CodeBlocksAreFenced)
{
    EXPECT_EQ(md_of(text_doc{paragraph("int x;\n  *y*;").set_style(builtin::preformatted)}),
        "```\nint x;\n  *y*;\n```\n");
    EXPECT_EQ(md_of(text_doc{paragraph("a\n```\nb").set_style(builtin::preformatted)}),
        "````\na\n```\nb\n````\n");
}

TEST(Markdown, LineBreaks)
{
    EXPECT_EQ(md_of(text_doc{paragraph("line 1\nline 2\n")}), "line 1\\\nline 2\n");
    EXPECT_EQ(md_of(text_doc{heading(2, "a\nb")}), "## a b\n");
}

TEST(Markdown, EscapesMarkup)
{
    EXPECT_EQ(md_of(text_doc{paragraph("*a* _b_ snake_case [c](d) <e> `f` \\ | ~ AT&T &amp;")}),
        "\\*a\\* \\_b\\_ snake_case \\[c\\](d) \\<e> \\`f\\` \\\\ \\| \\~ AT&T \\&amp;\n");
}

TEST(Markdown, EscapesLineStarts)
{
    EXPECT_EQ(md_of(text_doc{paragraph("# no")}), "\\# no\n");
    EXPECT_EQ(md_of(text_doc{paragraph("- no")}), "\\- no\n");
    EXPECT_EQ(md_of(text_doc{paragraph("+ no")}), "\\+ no\n");
    EXPECT_EQ(md_of(text_doc{paragraph("> no")}), "\\> no\n");
    EXPECT_EQ(md_of(text_doc{paragraph("12. no")}), "12\\. no\n");
    EXPECT_EQ(md_of(text_doc{paragraph("1) no")}), "1\\) no\n");
    EXPECT_EQ(md_of(text_doc{paragraph("2024 was")}), "2024 was\n");
    EXPECT_EQ(md_of(text_doc{paragraph("a\n- b")}), "a\\\n\\- b\n");
    EXPECT_EQ(md_of(text_doc{paragraph("    indented")}), "&#32;   indented\n");
    EXPECT_EQ(md_of(text_doc{heading(1, "C# #")}), "# C\\# \\#\n");
}

TEST(Markdown, Links)
{
    EXPECT_EQ(md_of(text_doc{paragraph(hyperlink("https://example.com", "site"),
                  " ",
                  hyperlink("https://example.com"),
                  hyperlink("https://example.com", "https://example.com"),
                  " ",
                  hyperlink("a b.html", "spaced"),
                  " ",
                  hyperlink("f(x).html", "x"))}),
        "[site](https://example.com) [](https://example.com)<https://example.com> "
        "[spaced](<a b.html>) [x](f(x).html)\n");
}

TEST(Markdown, BookmarksAreAnchors)
{
    EXPECT_EQ(md_of(text_doc{heading(1, bookmark{"intro part"}, "Intro"),
                  paragraph("See ", hyperlink(bookmark{"intro part"}, "intro"), ".")}),
        "# <a id=\"intro_part\"></a>Intro\n\nSee [intro](#intro_part).\n");
}

TEST(Markdown, ListsFollowListStyles)
{
    text_doc doc;
    const auto num = doc.add_style(numbered("N", 3));
    doc.add(list(list_item("a", list(list_item("nested"))), list_item("b")).set_style(num));
    doc.add(paragraph("between"));
    doc.add(list(list_item("x"), list_item("y")));
    EXPECT_EQ(md_of(doc), "3. a\n   - nested\n4. b\n\nbetween\n\n- x\n- y\n");
}

TEST(Markdown, LooseListItems)
{
    EXPECT_EQ(md_of(text_doc{list(list_item("a", "a2"), list_item("b"))}), "- a\n\n  a2\n\n- b\n");
    EXPECT_EQ(md_of(text_doc{list(list_item(paragraph("x\ny").set_style(builtin::preformatted)))}),
        "- ```\n  x\n  y\n  ```\n");
    EXPECT_EQ(md_of(text_doc{list(list_item())}), "-\n");
}

TEST(Markdown, AdjacentListsStayApart)
{
    EXPECT_EQ(
        md_of(text_doc{list(list_item("a")), list(list_item("b"))}), "- a\n\n<!-- -->\n\n- b\n");
}

TEST(Markdown, Tables)
{
    table t(3, 2);
    t.row(0).assign("Name", "Result");
    t.row(1).assign("a|b", "pass");
    t.row(2).assign("c", "");
    t.set_header_rows(1);
    EXPECT_EQ(md_of(text_doc{t}), "| Name | Result |\n| --- | --- |\n| a\\|b | pass |\n| c | |\n");

    t.set_header_rows(0);
    EXPECT_EQ(
        md_of(text_doc{t}), "| | |\n| --- | --- |\n| Name | Result |\n| a\\|b | pass |\n| c | |\n");
}

TEST(Markdown, TableCellsWithSpansAndBlocks)
{
    table t(2, 2);
    t.cell(0, 0) = table_cell("wide").set_span(1, 2);
    t.cell(1, 0) = table_cell("p1", "p2\nline", list(list_item("i1"), list_item("i2")));
    t.cell(1, 1) = table_cell(paragraph(span("x|y").set_style(builtin::source_text)));
    t.set_header_rows(1);
    EXPECT_EQ(md_of(text_doc{t}),
        "| wide | |\n| --- | --- |\n| p1<br>p2<br>line<br>i1<br>i2 | `x\\|y` |\n");
}

TEST(Markdown, ImagesAndBlockFrames)
{
    text_doc doc;
    const image img = doc.add_image("Pictures/a.png", "png", "image/png");
    doc.add(frame(img));
    doc.add(paragraph("inline ", frame(image{"https://x.org/b c.png"})));
    EXPECT_EQ(md_of(doc), "![](Pictures/a.png)\n\ninline ![](<https://x.org/b c.png>)\n");

    md::file f(doc);
    f.resource_dir = "assets";
    EXPECT_EQ(f.to_string(), "![](assets/Pictures/a.png)\n\ninline ![](<https://x.org/b c.png>)\n");
}

TEST(Markdown, SaveWritesResources)
{
    text_doc doc;
    doc.add(frame(doc.add_image("Pictures/a.png", "png bytes", "image/png")));
    const fs::path out = "md/out/save/report.md";
    fs::remove_all(out.parent_path());
    md::file(doc).save(out);

    EXPECT_EQ(read(out), "![](Pictures/a.png)\n");
    EXPECT_EQ(read("md/out/save/Pictures/a.png"), "png bytes");
}

TEST(Markdown, QuotesPageBreaksAndUnknown)
{
    EXPECT_EQ(md_of(text_doc{paragraph("q1").set_style("Quotations"),
                  paragraph("q2\nmore").set_style("Quotations"),
                  page_break{},
                  unknown{"<text:p/>"},
                  paragraph("after")}),
        "> q1\n>\n> q2\\\n> more\n\n---\n\nafter\n");
}

TEST(Markdown, FromOdt)
{
    const auto md = md::file(odt::file::load("odt/table-intermediate.odt").doc).to_string();
    EXPECT_NE(md.find("| --- |"), std::string::npos);
}

