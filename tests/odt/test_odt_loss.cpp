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

// What a load -> save of an ODT file loses, case by case: ODF the model
// doesn't hold. Each test records what happens today, not what should, and
// says what is still to decide (TODO.md, Correctness). Deciding a case means
// changing its test to the behaviour chosen.
//
// The choices are to model it (all formats then gain it), to drop it on
// purpose, or to carry it over as is. Carrying it over is only safe for markup
// nothing in the model can contradict; otherwise an edit leaves the two
// disagreeing in the saved file.

#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "bytes.h"
#include "docsmithcpp/odt/file.h"
#include "odf/package.h"

using namespace docsmith;

namespace
{
/// Writes basic.odt with body (ODF markup) as its office:text, loads that with
/// odt::file and saves it again. Holds the second save's content.xml.
struct resaved
{
    resaved(const std::string &name, const std::string &body)
    {
        auto pkg = odf::package::read("odt/basic.odt");
        pugi::xml_document content;
        const auto *e = pkg.find("content.xml");
        content.load_buffer(e->data.data(), e->data.size());
        auto text = content.select_node("//office:text").node();
        while(text.first_child())
            text.remove_child(text.first_child());
        // pugixml matches names by prefix, so the body needs no declarations.
        pugi::xml_document frag;
        EXPECT_TRUE(frag.load_string(("<body>" + body + "</body>").c_str())) << body;
        for(auto c : frag.first_child().children())
            text.append_copy(c);
        std::ostringstream ss;
        content.save(ss, "", pugi::format_raw);
        pkg.set("content.xml", ss.str(), "text/xml");
        pkg.set("Pictures/smiley.png", detail::read_bytes("odt/smiley.png"), "image/png");

        const std::string in = "odt/out/loss_" + name + ".odt";
        const std::string out = "odt/out/loss_" + name + "_resaved.odt";
        pkg.write(in);
        odt::file::load(in).save(out);
        const auto saved = odf::package::read(out);
        const auto *c = saved.find("content.xml");
        xml.load_buffer(c->data.data(), c->data.size());
    }

    pugi::xml_node node(const char *xpath) const { return xml.select_node(xpath).node(); }

    pugi::xml_document xml;
};

const std::string smiley = R"(<draw:image xlink:href="Pictures/smiley.png"/>)";
} // namespace

// A cell can hold a value (office:value-type, office:value) and a formula
// (table:formula) as well as its paragraphs. Writer shows the paragraphs: a
// value with none shows an empty cell, though a formula can still use it. A
// formula cell holds the formula, its result as a number and the result as
// text, so carrying any of these over as is would let them disagree with the
// text once it's edited.
// Decided: cells are text. The value and formula are dropped, so a formula
// cell becomes its last result. Modelling them is a TODO.md item (Model gaps).
TEST(ODT_Loss, CellValuesAndFormulasAreDropped)
{
    // A number cell; a value with no text; a formula using it, as LibreOffice
    // saves one.
    const resaved r("cell_value",
        R"(<table:table table:name="T"><table:table-column/>)"
        R"(<table:table-row><table:table-cell office:value-type="float" office:value="123">)"
        R"(<text:p>123</text:p></table:table-cell></table:table-row>)"
        R"(<table:table-row><table:table-cell office:value-type="float" office:value="123"/>)"
        R"(</table:table-row>)"
        R"(<table:table-row><table:table-cell table:formula="ooow:&lt;A2&gt;" )"
        R"(office:value-type="float" office:value="123"><text:p>123</text:p>)"
        R"(</table:table-cell></table:table-row></table:table>)");
    const auto cells = r.xml.select_nodes("//table:table-cell");
    ASSERT_EQ(cells.size(), 3U);
    for(const auto &c : cells)
    {
        EXPECT_FALSE(c.node().attribute("office:value-type"));
        EXPECT_FALSE(c.node().attribute("office:value"));
        EXPECT_FALSE(c.node().attribute("table:formula"));
    }
    EXPECT_STREQ(cells[0].node().child("text:p").text().as_string(), "123");
    EXPECT_STREQ(cells[1].node().child("text:p").text().as_string(), "") << "empty, as shown";
    EXPECT_STREQ(cells[2].node().child("text:p").text().as_string(), "123") << "its result";
}

// A frame's anchor goes with its position (svg:x, svg:y) and how text wraps
// round it, which is in its graphic style. Keeping some of these and not the
// others would move the picture.
// To decide: model anchoring and position together, or keep frames inline.
TEST(ODT_Loss, FrameAnchorAndPositionAreDropped)
{
    const resaved r("frame_anchor",
        R"(<text:p><draw:frame text:anchor-type="paragraph" svg:x="2cm" svg:y="1cm" )"
        R"(svg:width="1cm" svg:height="1cm">)" +
            smiley + "</draw:frame></text:p>");
    auto f = r.node("//draw:frame");
    EXPECT_TRUE(f.attribute("svg:width")) << "the size is modelled";
    EXPECT_FALSE(f.attribute("text:anchor-type"));
    EXPECT_FALSE(f.attribute("svg:x"));
    EXPECT_FALSE(f.attribute("svg:y"));
}

// An image's alternative text. Every format has it (DOCX wp:docPr descr, HTML
// alt), so it would be a model field; HTML writes alt="" today.
// Decided, not done yet: frame::accessibility_title and
// frame::accessibility_description (TODO.md, Correctness).
TEST(ODT_Loss, ImageTitleAndDescriptionAreDropped)
{
    const resaved r("image_alt",
        R"(<text:p><draw:frame svg:width="1cm" svg:height="1cm">)" + smiley +
            "<svg:title>Smiley</svg:title><svg:desc>A smiling face</svg:desc>"
            "</draw:frame></text:p>");
    EXPECT_TRUE(r.node("//draw:frame"));
    EXPECT_FALSE(r.node("//draw:frame/svg:title"));
    EXPECT_FALSE(r.node("//draw:frame/svg:desc"));
}

// A table's title and description, for accessibility (DOCX tblCaption and
// tblDescription).
// Decided, not done yet: table::accessibility_title and
// table::accessibility_description, like the image's.
TEST(ODT_Loss, TableTitleAndDescriptionAreDropped)
{
    const resaved r("table_title",
        R"(<table:table table:name="T"><table:title>Results</table:title>)"
        R"(<table:desc>One row per test</table:desc><table:table-column/>)"
        R"(<table:table-row><table:table-cell><text:p>x</text:p></table:table-cell>)"
        R"(</table:table-row></table:table>)");
    EXPECT_TRUE(r.node("//table:table"));
    EXPECT_FALSE(r.node("//table:table/table:title"));
    EXPECT_FALSE(r.node("//table:table/table:desc"));
}

// Where a link opens (HTML target, DOCX w:tgtFrame), and the link's own
// character styles.
// To decide: model the target frame; the styles go with character styles on
// links generally.
TEST(ODT_Loss, LinkTargetFrameAndStylesAreDropped)
{
    const resaved r("link_target",
        R"(<text:p><text:a xlink:type="simple" xlink:href="https://example.com/" )"
        R"(office:target-frame-name="_blank" xlink:show="new" )"
        R"(text:style-name="Internet_20_link">site</text:a></text:p>)");
    auto a = r.node("//text:a");
    EXPECT_STREQ(a.attribute("xlink:href").as_string(), "https://example.com/");
    EXPECT_FALSE(a.attribute("office:target-frame-name"));
    EXPECT_FALSE(a.attribute("text:style-name"));
}

// Element ids are what other markup refers to: RDF metadata in manifest.rdf,
// text:continue-list. Carried over as is, a copied paragraph would have the
// same id as the original, which ODF doesn't allow. The model holds neither
// RDF nor list references (text:continue-list is read as continue_numbering),
// so an id alone would be kept half modelled.
// Decided: dropped. A writer that needs an id generates it when saving.
TEST(ODT_Loss, ElementIdsAreDropped)
{
    const resaved r("element_id", R"(<text:p xml:id="para1">x</text:p>)");
    EXPECT_STREQ(r.node("//office:text/text:p").text().as_string(), "x");
    EXPECT_FALSE(r.node("//office:text/text:p").attribute("xml:id"));
}

// A column's default cell style applies to its cells that have no style of
// their own, so dropping it changes how those cells look.
// To decide: resolve it into those cells' styles when reading.
TEST(ODT_Loss, ColumnDefaultCellStyleIsDropped)
{
    const resaved r("column_default",
        R"(<table:table table:name="T">)"
        R"(<table:table-column table:default-cell-style-name="Shaded"/>)"
        R"(<table:table-row><table:table-cell><text:p>x</text:p></table:table-cell>)"
        R"(</table:table-row></table:table>)");
    EXPECT_FALSE(r.node("//table:table-column").attribute("table:default-cell-style-name"));
    EXPECT_FALSE(r.node("//table:table-cell").attribute("table:style-name"));
}

// Soft page breaks are where LibreOffice last laid out a page break. They are
// a cache: LibreOffice makes them again when it lays the document out.
// Decided: dropped, wherever they are (between blocks, in a paragraph, in a
// list or a table).
TEST(ODT_Loss, SoftPageBreaksAreDropped)
{
    const resaved r("soft_page_break",
        "<text:p>before</text:p><text:soft-page-break/>"
        "<text:p>split <text:soft-page-break/>here</text:p>"
        "<text:list><text:list-item><text:p>a</text:p></text:list-item><text:soft-page-break/>"
        "<text:list-item><text:p>b</text:p></text:list-item></text:list>"
        R"(<table:table table:name="T"><table:table-column/><table:table-row>)"
        "<table:table-cell><text:p>x</text:p></table:table-cell></table:table-row>"
        "<text:soft-page-break/><table:table-row><table:table-cell><text:p>y</text:p>"
        "</table:table-cell></table:table-row></table:table>");
    EXPECT_EQ(r.xml.select_nodes("//text:list-item").size(), 2U);
    EXPECT_EQ(r.xml.select_nodes("//table:table-row").size(), 2U);
    EXPECT_FALSE(r.node("//text:soft-page-break"));
    EXPECT_STREQ(r.node("//office:text/text:p[2]").text().as_string(), "split here")
        << "the text either side joins up";
}

// A list item's label as last laid out (text:number) is also a cache, and
// kept as is it would go stale when items are added or moved.
// Decided: dropped, like soft page breaks.
TEST(ODT_Loss, ListLabelCacheIsDropped)
{
    const resaved r("list_label",
        "<text:list><text:list-item><text:number>1.</text:number><text:p>a</text:p>"
        "</text:list-item></text:list>");
    EXPECT_FALSE(r.node("//text:number"));
    EXPECT_STREQ(r.node("//text:list-item/text:p").text().as_string(), "a");
}

// An attribute in a namespace the writer doesn't declare (another
// application's extension). Carrying it over means declaring its namespace
// too, which the ODT reader can't yet tell (TODO.md, Namespace prefixes in
// ODT).
// To decide: whether foreign attributes are kept at all.
TEST(ODT_Loss, ForeignAttributesAreDropped)
{
    const resaved r("foreign_attribute", R"(<text:p xmlns:x="urn:example" x:flag="1">x</text:p>)");
    EXPECT_STREQ(r.node("//office:text/text:p").text().as_string(), "x");
    EXPECT_FALSE(r.node("//office:text/text:p").attribute("x:flag"));
}
