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
#include "docsmithcpp/odt/file.h"

using namespace docsmith;
namespace fs = std::filesystem;

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

TEST(HTML, BoldAndColourBecomeCss)
{
    text_doc doc;
    const auto pass =
        doc.add_style(style("Pass", text_props(font_weight::bold, colour::rgb(0, 128, 255))));
    doc.add(paragraph("ok").set_style(pass));

    EXPECT_TRUE(has(html::file(doc).to_string(), ".Pass { font-weight: bold; color: #0080ff; }"));
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
        paragraph(hyperlink(bookmark{"Bookmark 1"}, "back"))};
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

TEST(HTML, ImageFilesAreCopiedNextToThePage)
{
    // The test runs in tests/; the page goes to tests/html/out/.
    fs::remove_all("html/out/disk_image");
    text_doc doc{paragraph(frame(image{"odt/smiley.png"}))};
    EXPECT_TRUE(has(html::file(doc).to_string(), R"(src="Pictures/smiley.png")"));

    html::file(doc).save("html/out/disk_image/image.html");
    EXPECT_TRUE(
        has(read("html/out/disk_image/image.html"), R"(<img src="Pictures/smiley.png" alt="">)"));
    EXPECT_EQ(read("html/out/disk_image/Pictures/smiley.png"), read("odt/smiley.png"));
}

TEST(HTML, ImageFileInsideResourceDirIsReferencedInPlace)
{
    fs::create_directories("html/out/in_place/img/sub");
    fs::copy_file(
        "odt/smiley.png", "html/out/in_place/img/sub/s.png", fs::copy_options::overwrite_existing);

    html::file page(text_doc{paragraph(frame(image{"html/out/in_place/img/sub/s.png"}))});
    page.resource_dir = "img";
    page.save("html/out/in_place/page.html");

    EXPECT_TRUE(has(read("html/out/in_place/page.html"), R"(src="img/sub/s.png")"));
    EXPECT_FALSE(fs::exists("html/out/in_place/img/Pictures"));
}

TEST(HTML, SameFileNamesFromDifferentFoldersAreKeptApart)
{
    fs::create_directories("html/out/clash/src");
    fs::copy_file(
        "odt/smiley.png", "html/out/clash/src/smiley.png", fs::copy_options::overwrite_existing);

    html::file page(text_doc{paragraph(frame(image{"odt/smiley.png"})),
        paragraph(frame(image{"html/out/clash/src/smiley.png"}))});
    page.resource_dir = "res";
    page.save("html/out/clash/page.html");

    const auto html = read("html/out/clash/page.html");
    EXPECT_TRUE(has(html, R"(src="res/Pictures/smiley.png")"));
    EXPECT_TRUE(has(html, R"(src="res/Pictures/smiley_2.png")"));
    EXPECT_TRUE(fs::exists("html/out/clash/res/Pictures/smiley_2.png"));
}

TEST(HTML, ParsedOdtImagesLandUnderResourceDir)
{
    const text_doc doc = odt::file::load("odt/image.odt").doc;
    const std::string name = "Pictures/100000000000002A0000002A13903900F3783A21.png";
    const std::string bytes = doc.resources().find(name)->data();

    fs::remove_all("html/out/odt_image");
    html::file(doc).save("html/out/odt_image/default.html");
    EXPECT_TRUE(has(read("html/out/odt_image/default.html"), "src=\"" + name + "\""));
    EXPECT_EQ(read("html/out/odt_image/" + name), bytes);

    html::file page(doc);
    page.resource_dir = "default_files";
    EXPECT_TRUE(has(page.to_string(), "src=\"default_files/" + name + "\""));
    page.save("html/out/odt_image/other.html");
    EXPECT_EQ(read("html/out/odt_image/default_files/" + name), bytes);
}

TEST(HTML, ResourceNamesCannotLeaveTheFolder)
{
    text_doc doc;
    doc.add(paragraph(frame(doc.add_image("../escape.png", "x", "image/png"))));
    EXPECT_THROW(html::file(doc).save("html/out/escape/page.html"), std::runtime_error);
    EXPECT_FALSE(fs::exists("html/out/escape.png"));
}

TEST(HTML, UrlImagesAreLeftAsIs)
{
    const auto html =
        html::file(text_doc{paragraph(frame(image{"https://example.com/a.png"}))}).to_string();
    EXPECT_TRUE(has(html, R"(src="https://example.com/a.png")"));
}

TEST(HTML, ConvertsParsedOdt)
{
    html::file(odt::file::load("odt/moderate.odt").doc).save("html/out/moderate.html");
    const auto html = read("html/out/moderate.html");

    EXPECT_TRUE(has(html, "Heading 1</h1>"));
    EXPECT_TRUE(has(html,
        R"(<a class="Internet_20_link" href="https://github.com/MichaelCoutlakis/docsmithcpp">docsmith</a>)"))
        << "the link keeps LibreOffice's link style";
    EXPECT_TRUE(has(html, ".Internet_20_link { color: #000080; }"));
    EXPECT_EQ(count(html, "<li"), 8U); // 2 + 2 + 1 + (1 with a nested 1) + 1
}

TEST(HTML, MetadataGoesInTheHead)
{
    text_doc doc{heading(1, "Heading")};
    doc.meta = {.title = "Doc & Title",
        .author = "A. Writer",
        .language = "en-ZA",
        .description = "Say \"hi\"",
        .keywords = {"one", "two"}};
    const auto html = html::file(doc).to_string();

    EXPECT_TRUE(has(html, R"(<html lang="en-ZA">)"));
    EXPECT_TRUE(has(html, "<title>Doc &amp; Title</title>")) << "the document title before the heading";
    EXPECT_TRUE(has(html, R"(<meta name="author" content="A. Writer">)"));
    EXPECT_TRUE(has(html, R"(<meta name="description" content="Say &quot;hi&quot;">)"));
    EXPECT_TRUE(has(html, R"(<meta name="keywords" content="one, two">)"));

    html::file page(doc);
    page.title = "Override";
    EXPECT_TRUE(has(page.to_string(), "<title>Override</title>"));
}

TEST(HTML, NoMetadataNoMetaTags)
{
    const auto html = html::file(text_doc{paragraph("x")}).to_string();
    EXPECT_TRUE(has(html, "<html>\n"));
    EXPECT_EQ(count(html, "<meta "), 1U); // charset only
}

TEST(HTML, TitleParagraph)
{
    text_doc doc{heading(1, "Heading")};
    doc.set_title("Report");
    const auto html = html::file(doc).to_string();

    EXPECT_TRUE(has(html, "<title>Report</title>")) << "the title paragraph before the heading";
    EXPECT_TRUE(has(html, R"(<p class="Title">Report</p>)"));
    EXPECT_FALSE(has(html, ".Title {")) << "styling is left to html::file::css";
}

TEST(HTML, UrlTargetsAreLeftAsIs)
{
    // A URL that starts with "#" is not a bookmark target, so its id isn't sanitised.
    const auto html = html::file(text_doc{paragraph(hyperlink("#a b", "x"))}).to_string();
    EXPECT_TRUE(has(html, R"(<a href="#a b">x</a>)"));
}

TEST(HTML, ParsedOdtHasStylesAndNumbering)
{
    const auto html = html::file(odt::file::load("odt/moderate.odt").doc).to_string();

    EXPECT_TRUE(has(html, R"(<ol class="L1" start="7">)"));
    EXPECT_TRUE(has(html, R"(<ul class="L2">)"));
    EXPECT_TRUE(has(html, R"(<ol class="L3" type="a" start="3">)"));
    // P3's own definition only sets rsids; the rest comes from its parents,
    // the spacing and keep-with-next from LibreOffice's Heading.
    EXPECT_TRUE(has(html,
        R"(.P3 { font-size: 18pt; font-family: "Liberation Sans"; font-weight: bold; )"
        R"(margin-top: 11.991pt; margin-bottom: 6.009pt; break-after: avoid; })"))
        << html;
}

TEST(HTML, ParagraphSpacingAndAlignment)
{
    text_doc doc;
    doc.add_style(style("Base",
        style_family::paragraph,
        paragraph_props(space_before{pt(12)}, text_align::justify, keep_together{})));
    style child("Note",
        style_family::paragraph,
        paragraph_props(text_align::centre, indent_start{pt(18)}, first_line_indent{pt(-9)},
            keep_with_next{}));
    child.m_parent_style = "Base";
    doc.add_style(child);
    doc.add(paragraph("x").set_style("Note"));
    const auto html = html::file(doc).to_string();
    EXPECT_TRUE(has(html,
        ".Note { margin-top: 12pt; margin-left: 18pt; text-indent: -9pt; text-align: center; "
        "break-after: avoid; break-inside: avoid; }"))
        << html;
}

TEST(HTML, CssIncludesInheritedProperties)
{
    text_doc doc;
    doc.add_style(style("Base", text_props(font_style::italic, font_size(10))));
    style child("Child", text_props(font_size(12)));
    child.m_parent_style = "Base";
    doc.add_style(child);
    const auto html = html::file(doc).to_string();
    EXPECT_TRUE(has(html, ".Child { font-size: 12pt; font-style: italic; }"));
}

TEST(HTML, HeaderRows)
{
    table t = table::from_cells({{{"Name"}, {"Value"}}, {{"a"}, {"1"}}}).set_header_rows(1);
    const auto html = html::file(text_doc{t}).to_string();

    EXPECT_TRUE(has(html, "<table>\n  <thead>\n    <tr>\n      <th>Name</th>"));
    EXPECT_TRUE(has(html, "</tr>\n  </thead>\n  <tbody>\n    <tr>\n      <td>a</td>"));
    EXPECT_TRUE(has(html, "</tr>\n  </tbody>\n</table>"));
    EXPECT_EQ(count(html, "<th>"), 2U);

    // Without header rows, no sections.
    const auto plain = html::file(text_doc{table(1, 1)}).to_string();
    EXPECT_FALSE(has(plain, "<thead>") || has(plain, "<tbody>"));

    // Only header rows.
    const auto only = html::file(text_doc{table(1, 1).set_header_rows(1)}).to_string();
    EXPECT_TRUE(has(only, "</thead>\n</table>"));
}

TEST(HTML, WhiteSpace)
{
    const auto html = html::file(text_doc{paragraph("a  b\tc\nd   e")}).to_string();
    EXPECT_TRUE(has(
        html, "<p>a&nbsp; b<span style=\"white-space: pre\">\t</span>c<br>d&nbsp;&nbsp; e</p>"));
}

TEST(HTML, PageBreak)
{
    const auto html =
        html::file(text_doc{paragraph("one"), page_break{}, paragraph("two")}).to_string();
    EXPECT_TRUE(has(html, "<p>one</p>\n<div style=\"break-after: page\"></div>\n<p>two</p>"));
}

TEST(HTML, Preformatted)
{
    text_doc doc{paragraph("\n<a>\n  b\tc").set_style(builtin::preformatted),
        paragraph("Run ", span("make").set_style(builtin::source_text), ".")};
    style log("Log");
    log.m_parent_style = builtin::preformatted;
    doc.add_style(log);
    doc.add(paragraph("x  y").set_style(log));
    const auto html = html::file(doc).to_string();

    // As is inside <pre>; the extra newline keeps the leading one.
    EXPECT_TRUE(has(html, "<pre class=\"Preformatted_20_Text\">\n\n&lt;a&gt;\n  b\tc</pre>"));
    EXPECT_TRUE(has(html, "<p>Run <code class=\"Source_20_Text\">make</code>.</p>"));
    EXPECT_TRUE(has(html, "<pre class=\"Log\">x  y</pre>")) << "a style based on it";
}

TEST(HTML, Fields)
{
    text_doc doc{paragraph("[",
        field{field_type::title},
        "] page ",
        field{field_type::page_number},
        " of ",
        field{field_type::page_count})};
    doc.meta.title = "Report";
    EXPECT_TRUE(has(html::file(doc).to_string(), "<p>[Report] page  of </p>"))
        << "no pages, so no page numbers";
}

TEST(HTML, HeaderAndFooter)
{
    text_doc doc{paragraph("Body")};
    doc.page.set_header(paragraph("Top"));
    doc.page.set_footer(paragraph("Bottom"));
    EXPECT_TRUE(has(html::file(doc).to_string(),
        "<body>\n<header>\n  <p>Top</p>\n</header>\n<p>Body</p>\n<footer>\n  "
        "<p>Bottom</p>\n</footer>\n</body>"));
    EXPECT_FALSE(has(html::file(text_doc{paragraph("x")}).to_string(), "<header>"));
}

TEST(HTML, TableOfContents)
{
    const text_doc doc{table_of_contents("Contents"),
        heading(1, "Intro"),
        heading(2, bookmark{"r"}, "Results"),
        heading(2, "Intro"),
        heading(1, "End")};
    const auto html = html::file(doc).to_string();
    EXPECT_TRUE(has(html,
        "<nav class=\"toc\">\n"
        "  <p class=\"toc-title\">Contents</p>\n"
        "  <ul>\n"
        "    <li><a href=\"#intro\">Intro</a>\n"
        "      <ul>\n"
        "        <li><a href=\"#r\">Results</a>\n"
        "        </li>\n"
        "        <li><a href=\"#intro-2\">Intro</a>\n"
        "        </li>\n"
        "      </ul>\n"
        "    </li>\n"
        "    <li><a href=\"#end\">End</a>\n"
        "    </li>\n"
        "  </ul>\n"
        "</nav>\n"))
        << html;
    EXPECT_TRUE(has(html, "<h1 id=\"intro\">Intro</h1>"));
    EXPECT_TRUE(has(html, "<h2><a id=\"r\"></a>Results</h2>")) << "linked by its bookmark";
    EXPECT_TRUE(has(html, "<h2 id=\"intro-2\">Intro</h2>"));

    EXPECT_TRUE(has(html::file(text_doc{heading(1, "A")}).to_string(), "<h1>A</h1>"))
        << "no ids without a table of contents";
}

namespace
{
/// A report-style table: a styled header, lines and column widths.
text_doc formatted_table()
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
            header_cells{cell_props(colour::rgb(0xdd, 0xdd, 0xdd),
                cell_borders{.m_bottom = thick},
                paragraph_style{"Table Heading"})})));
    doc.add_style(style("Table Heading", style_family::paragraph, text_props(font_weight::bold)));
    table t = table::from_cells({{"Test", "Result"}, {"open", "pass"}, {"save", "fail"}});
    t.set_header_rows(1).set_style("Grid").set_column_widths({3, 1});
    t.cell(2, 1).props.set(colour::rgb(0xff, 0xcc, 0xcc));
    doc.add(t);
    return doc;
}
} // namespace

TEST(HTML, TableFormatting)
{
    const auto html = html::file(formatted_table()).to_string();
    EXPECT_TRUE(has(html,
        ".Grid { border-collapse: collapse; width: 80%; margin-left: auto; margin-right: auto; }"));
    EXPECT_TRUE(has(html, "<table class=\"Grid\">"));
    EXPECT_TRUE(
        has(html, "<colgroup><col style=\"width: 75%;\"><col style=\"width: 25%;\"></colgroup>"));
    // The header: its background and the line below it, and its text's style.
    EXPECT_TRUE(has(html,
        "<th class=\"Table_Heading\" style=\"border-top: 1.5pt solid #204060; "
        "border-bottom: 1.5pt solid #204060; border-left: 1.5pt solid #204060; "
        "background-color: #dddddd;\">Test</th>"));
    // A body cell between rows, and one with direct formatting.
    EXPECT_TRUE(has(html,
        "<td style=\"border-top: 1.5pt solid #204060; border-bottom: 0.5pt solid #000000; "
        "border-left: 1.5pt solid #204060;\">open</td>"));
    EXPECT_TRUE(has(html, "background-color: #ffcccc;\">fail</td>"));

    // Direct borders on a table with no style still collapse.
    table plain(1, 1);
    plain.set_borders(border::solid());
    EXPECT_TRUE(has(
        html::file(text_doc{plain}).to_string(), "<table style=\"border-collapse: collapse;\">"));
}

TEST(HTML, ListNumbering)
{
    list_style num("N", {list_style_num(style{}, 1, list_enum::arabic)});
    list l(list_item("header"), list_item("one"), list_item("five"), list_item("six"));
    l.set_style(num);
    l.items()[0].numbered = false;
    l.items()[2].start_value = 5;
    list more(list_item("seven"));
    more.set_style(num);
    more.continue_numbering = true;
    text_doc doc{l, paragraph("between"), more};
    doc.list_styles().add(num);
    const auto html = html::file(doc).to_string();

    // <ol> counts the unnumbered item too, so "one" says its number.
    EXPECT_TRUE(has(html, R"(<li style="list-style-type: none">header</li>)"));
    EXPECT_TRUE(has(html, R"(<li value="1">one</li>)"));
    EXPECT_TRUE(has(html, R"(<li value="5">five</li>)"));
    EXPECT_TRUE(has(html, "<li>six</li>"));
    EXPECT_TRUE(has(html, R"(<ol class="N" start="7">)"));
}

TEST(HTML, StyledLink)
{
    const std::string html =
        html::file(text_doc{paragraph(hyperlink("https://example.com", "site").set_style("Quiet"))})
            .to_string();
    EXPECT_TRUE(has(html, R"(<a class="Quiet" href="https://example.com">site</a>)")) << html;
}

TEST(HTML, AlternativeText)
{
    frame f(image{"a.png"});
    f.accessibility_title = "A \"dot\"";
    f.accessibility_description = "Long";
    table t = table::from_cells({{"x"}});
    t.accessibility_description = "Results & more";
    const std::string html = html::file(text_doc{paragraph(f), t}).to_string();
    EXPECT_TRUE(has(html, R"(<img src="a.png" alt="A &quot;dot&quot;">)")) << html;
    EXPECT_TRUE(has(html, R"(<table aria-label="Results &amp; more">)")) << html;
}

TEST(HTML, ImageSize)
{
    frame f(image{"https://example.com/a.png"});
    f.width = pt(36);
    EXPECT_TRUE(has(html::file(text_doc{paragraph(f)}).to_string(),
        R"(<img src="https://example.com/a.png" style="width: 36pt;" alt="">)"));
}
