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
    doc.styles().insert(text_style("Bold", text_props(font_weight::bold)));
    doc.styles().insert(text_style("Italic", text_props(font_style::italic)));
    doc.styles().insert(
        text_style("BoldItalic", text_props(font_weight::bold, font_style::italic)));
    text_style child("Child");
    child.parent = "Bold";
    doc.styles().insert(child);
    doc.styles().insert(text_style("Red", text_props(colour::rgb(255, 0, 0))));
    return doc;
}

list_style numbered(std::string name, int start = 1)
{
    return list_style(name,
        {number_level{.level = 1, .format = list_enum::arabic, .suffix = ".", .start_from = start},
            bullet_level{.level = 2, .bullet = bullet_type::bullet()}});
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
    const auto num = doc.styles().insert(numbered("N", 3)).first;
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

TEST(Markdown, TableOfContents)
{
    EXPECT_EQ(md_of(text_doc{table_of_contents("Contents"),
                  heading(1, "Intro"),
                  heading(2, "Scope and aims"),
                  heading(3, "Deep"),
                  heading(1, bookmark{"res"}, "Results"),
                  heading(4, "Too deep")}),
        "Contents\n\n"
        "- [Intro](#intro)\n"
        "  - [Scope and aims](#scope-and-aims)\n"
        "    - [Deep](#deep)\n"
        "- [Results](#res)\n\n"
        "# <a id=\"intro\"></a>Intro\n\n"
        "## <a id=\"scope-and-aims\"></a>Scope and aims\n\n"
        "### <a id=\"deep\"></a>Deep\n\n"
        "# <a id=\"res\"></a>Results\n\n"
        "#### <a id=\"too-deep\"></a>Too deep\n");
}

TEST(Markdown, HeadingsGetNoAnchorsWithoutATableOfContents)
{ EXPECT_EQ(md_of(text_doc{heading(1, "Intro")}), "# Intro\n"); }

TEST(Markdown, FieldsAndPageHeaderAndFooter)
{
    text_doc doc{paragraph("Title: ",
        field{field_type::title},
        ", page ",
        field{field_type::page_number},
        " of ",
        field{field_type::page_count},
        ".")};
    doc.meta.title = "Report";
    // Markdown has no pages: page numbers, the header and the footer are left out.
    doc.page.set_header(paragraph("header"));
    doc.page.set_footer(paragraph("footer"));
    EXPECT_EQ(md_of(doc), "Title: Report, page  of .\n");

    const std::string date = md_of(text_doc{paragraph(field{field_type::date})});
    ASSERT_EQ(date.size(), 11u); // YYYY-MM-DD and a newline
    EXPECT_EQ(date[4], '-');
    EXPECT_EQ(date[7], '-');
}

TEST(Markdown, FromOdt)
{
    const auto md = md::file(odt::file::load("odt/table-intermediate.odt").doc).to_string();
    EXPECT_NE(md.find("| --- |"), std::string::npos);
}

// ---- Input ------------------------------------------------------------------

#ifdef DOCSMITHCPP_MARKDOWN

namespace
{
/// Markdown -> model -> Markdown gives the same text, and parsing that again
/// gives the same document.
void expect_round_trip(const std::string &markdown)
{
    const text_doc doc = md::parse(markdown);
    const std::string out = md_of(doc);
    EXPECT_EQ(out, markdown);
    EXPECT_EQ(md::parse(out), doc);
}
} // namespace

TEST(Markdown, ImageAltText)
{
    frame f(image{"a.png"});
    f.accessibility_description = "described [here]";
    EXPECT_EQ(md_of(text_doc{f}), "![described \\[here\\]](a.png)\n")
        << "the description, with no title";
    f.accessibility_title = "Title";
    EXPECT_EQ(md_of(text_doc{f}), "![Title](a.png)\n");
    expect_round_trip("![Title](a.png)\n");
}

TEST(MarkdownInput, Blocks)
{
    const text_doc doc = md::parse("# Title\n\nSome *text*.\n\n---\n\n```c++\nint x;\n```\n");
    ASSERT_EQ(doc.body.size(), 4U);
    EXPECT_EQ(doc.body[0], block_node{heading(1, "Title")});
    EXPECT_EQ(doc.body[1], block_node{paragraph("Some ", span("text").set_style("Emphasis"), ".")});
    EXPECT_EQ(doc.body[2], block_node{page_break{}});
    EXPECT_EQ(doc.body[3], block_node{paragraph("int x;").set_style(builtin::preformatted)});

    const text_style *em = doc.styles().text.find("Emphasis");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->text.m_font_style, font_style::italic);
}

TEST(MarkdownInput, InlineContent)
{
    const text_doc doc = md::parse("a **b** `c` [d](https://x.org) <https://y.org> ~~e~~ "
                                   "&amp; &#65; &unknown;\\\nf\ng<a id=\"m\"></a><span>h</span>");
    ASSERT_EQ(doc.body.size(), 1U);
    EXPECT_EQ(doc.body[0],
        block_node{paragraph("a ",
            span("b").set_style("Strong_20_Emphasis"),
            " ",
            span("c").set_style(builtin::source_text),
            " ",
            hyperlink("https://x.org", "d"),
            " ",
            hyperlink("https://y.org", "https://y.org"),
            " e & A &unknown;\nf g",
            bookmark{"m"},
            "h")});
}

TEST(MarkdownInput, ImagesAndBookmarkLinks)
{
    const text_doc doc = md::parse("![alt *text*](pic.png)\n\nsee ![](a.png) and [x](#m)\n");
    ASSERT_EQ(doc.body.size(), 2U);
    frame alt(image{"pic.png"});
    alt.accessibility_title = "alt text"; // its text, without the emphasis
    EXPECT_EQ(doc.body[0], block_node{alt});
    EXPECT_EQ(doc.body[1],
        block_node{
            paragraph("see ", frame(image{"a.png"}), " and ", hyperlink(bookmark{"m"}, "x"))});
}

TEST(MarkdownInput, ListsGetListStyles)
{
    const text_doc doc = md::parse("1. a\n   - b\n     1. c\n2. d\n\n- e\n\n5. f\n");
    ASSERT_EQ(doc.body.size(), 3U);
    const auto &outer = std::get<list>(doc.body[0].v);
    EXPECT_EQ(outer.style, list_style_name("Numbering_20_123"));
    const auto &inner = std::get<list>(outer.items()[0].children()[1].v);
    EXPECT_EQ(inner.style, list_style_name("List_20_1"));
    EXPECT_EQ(std::get<list>(inner.items()[0].children()[1].v).style,
        list_style_name("Numbering_20_123"));
    EXPECT_EQ(std::get<list>(doc.body[1].v).style, list_style_name("List_20_1"));
    EXPECT_EQ(std::get<list>(doc.body[2].v).style, list_style_name("Numbering_20_123_start_5"));
    EXPECT_EQ(outer.items()[1].children(), std::vector<block_node>{block_node{paragraph("d")}});

    // A nested list of the same kind continues its parent's style.
    const text_doc same = md::parse("- a\n  - b\n");
    EXPECT_TRUE(
        std::get<list>(std::get<list>(same.body[0].v).items()[0].children()[1].v).style.empty());
}

TEST(MarkdownInput, Tables)
{
    const text_doc doc = md::parse("| A | B |\n| --- | --- |\n| 1 | x<br>y |\n");
    const auto &t = std::get<table>(doc.body.at(0).v);
    EXPECT_EQ(t.header_rows(), 1U);
    ASSERT_EQ(t.num_rows(), 2U);
    EXPECT_EQ(t.cell(0, 0), table_cell("A"));
    EXPECT_EQ(t.cell(1, 1), table_cell("x\ny"));

    const text_doc headless_doc = md::parse("| | |\n| --- | --- |\n| 1 | 2 |\n");
    const auto &headless = std::get<table>(headless_doc.body.at(0).v);
    EXPECT_EQ(headless.header_rows(), 0U);
    EXPECT_EQ(headless.num_rows(), 1U);
}

TEST(MarkdownInput, QuotesAndHtmlBlocks)
{
    const text_doc doc = md::parse("> q1\n>\n> q2\n\n<div>\ndropped\n</div>\n\nafter\n");
    ASSERT_EQ(doc.body.size(), 3U);
    EXPECT_EQ(doc.body[0], block_node{paragraph("q1").set_style("Quotations")});
    EXPECT_EQ(doc.body[1], block_node{paragraph("q2").set_style("Quotations")});
    EXPECT_EQ(doc.body[2], block_node{paragraph("after")});
}

TEST(MarkdownInput, RoundTrip)
{
    expect_round_trip("# Title\n\nPlain *em* **strong** ***both*** `code` text.\n");
    expect_round_trip("Line 1\\\nline 2\n");
    expect_round_trip("\\# not a heading, \\*not em\\*, snake_case, \\[x\\], a \\| b\n");
    expect_round_trip("&#32;   leading spaces\n");
    expect_round_trip(
        "## <a id=\"sec\"></a>Section\n\nSee [the section](#sec) or [site](https://x.org).\n");
    expect_round_trip("<https://example.com>\n");
    expect_round_trip("```\nint main()\n{\n    return 0;\n}\n```\n");
    expect_round_trip("- a\n- b\n  - c\n- d\n");
    expect_round_trip("1. a\n2. b\n   1. c\n\n- x\n");
    expect_round_trip("3. three\n4. four\n");
    expect_round_trip("- a\n\n  more\n\n- b\n");
    expect_round_trip("> quoted\n>\n> two\n");
    expect_round_trip("| A | B |\n| --- | --- |\n| 1 | 2 |\n| | x |\n");
    expect_round_trip("| | |\n| --- | --- |\n| 1 | 2 |\n");
    expect_round_trip("![](pic.png)\n\nText ![](a.png) inline.\n");
    expect_round_trip("---\n\nafter the break\n");
    expect_round_trip("- a\n\n<!-- -->\n\n- b\n");
}

TEST(MarkdownInput, AppendsToADocument)
{
    text_doc doc{heading(1, "Report")};
    doc.append(md::parse("Some *notes*.\n"));
    doc.append(md::parse("More *notes*.\n")); // equal style definitions merge
    EXPECT_EQ(md_of(doc), "# Report\n\nSome *notes*.\n\nMore *notes*.\n");
}

TEST(MarkdownInput, LoadReadsImagesIntoResources)
{
    const fs::path dir = "md/out/load";
    fs::remove_all(dir);
    fs::create_directories(dir / "img");
    std::ofstream(dir / "img/a.png", std::ios::binary) << "png bytes";
    std::ofstream(dir / "in.md", std::ios::binary) << "# Doc\n\n![](img/a.png)\n";
    std::ofstream(dir / "missing.md", std::ios::binary) << "![](missing.png)\n";

    const auto f = md::file::load(dir / "in.md");
    const resource *r = f.doc.resources().find("img/a.png");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->data(), "png bytes");
    EXPECT_EQ(r->media_type(), "image/png");
    EXPECT_TRUE(md::file::load(dir / "missing.md").doc.resources().empty());

    // The images travel with the document into other formats.
    odt::file(f.doc).save(dir / "out.odt");
    const auto back = odt::file::load(dir / "out.odt");
    EXPECT_EQ(back.doc.resources().size(), 1U);
    html::file(f.doc).save(dir / "html/out.html");
    EXPECT_EQ(read(dir / "html/img/a.png"), "png bytes");
}

TEST(MarkdownInput, ToOdtAndBack)
{
    const text_doc doc =
        md::parse("# T\n\n*em* **strong** `code`\n\n1. a\n2. b\n\n> q\n\n```\nx\n```\n");
    const fs::path out = "md/out/to_odt.odt";
    fs::create_directories(out.parent_path());
    odt::file(doc).save(out);
    EXPECT_EQ(md_of(odt::file::load(out).doc), md_of(doc));
}

#endif

TEST(Markdown, ListNumbering)
{
    // Markdown has no unnumbered item: one at the start goes before the list,
    // a later one is more of the item before it.
    text_doc doc;
    const auto num = doc.styles().insert(numbered("N")).first;
    list l(list_item("header"), list_item("one"), list_item("more"), list_item("five"));
    l.set_style(num);
    l.items()[0].numbered = false;
    l.items()[2].numbered = false;
    l.items()[3].start_value = 5;
    list more(list_item("six"));
    more.set_style(num);
    more.continue_numbering = true;
    doc.add(l);
    doc.add(paragraph("between"));
    doc.add(more);
    EXPECT_EQ(md_of(doc), "header\n\n1. one\n\n   more\n\n5. five\n\nbetween\n\n6. six\n");
}
