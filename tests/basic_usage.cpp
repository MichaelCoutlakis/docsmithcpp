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

#include <iostream>

#include <gtest/gtest.h>

#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/text_doc.h"

using namespace docsmith;

TEST(BASIC_USAGE, CreateDocument)
{
    text_doc doc{
        paragraph(frame(image{"odt/smiley.png"}).set_style("fr1"), span("", "nested")),
    };

    io_writer w(std::cout);
    w(doc);

    odt::file(doc).save("odt/out/basic_generated.odt");
}

TEST(BASIC_USAGE, ValueSemantics)
{
    // Deep copy + structural equality come for free -- no clone(), no is_equal().
    text_doc doc{heading(1, "Title"), paragraph("body")};
    text_doc copy = doc;
    EXPECT_EQ(copy, doc);

    std::get<heading>(copy.body.front().v).level = 2;
    EXPECT_NE(copy, doc);
}

TEST(BASIC_USAGE, EqualityIncludesResources)
{
    text_doc a;
    a.add(paragraph(frame(a.add_image("Pictures/a.png", "bytes", "image/png"))));
    text_doc b = a;
    EXPECT_EQ(a, b);

    // Same name, same bytes, held separately: still equal.
    text_doc c;
    c.add(paragraph(frame(c.add_image("Pictures/a.png", "bytes", "image/png"))));
    EXPECT_EQ(a, c);

    c.add_image("Pictures/a.png", "other bytes", "image/png");
    EXPECT_EQ(a.body, c.body);
    EXPECT_NE(a, c);
}

TEST(BASIC_USAGE, ChainedAdd)
{
    // Children are added via .add() (chainable); nesting a same-type node uses
    // .add() too, so there is no copy-vs-child ambiguity.
    auto h = heading(2).set_style("H2").add("Title ").add(span("emphasis"));
    auto nested = span("outer ").add(span("inner"));

    EXPECT_EQ(h.level, 2);
    EXPECT_EQ(h.children().size(), 2U);
    EXPECT_EQ(nested.children().size(), 2U);
}

TEST(BASIC_USAGE, SetTitle)
{
    text_doc doc{paragraph("Body")};
    EXPECT_EQ(doc.find_title(), nullptr);

    doc.set_title("First");
    ASSERT_NE(doc.find_title(), nullptr);
    EXPECT_EQ(doc.body.front(), to_block(paragraph("First").set_style(builtin::title)));

    // A second call changes the title in place rather than adding another.
    doc.set_title("Second");
    EXPECT_EQ(doc.body.size(), 2U);
    EXPECT_EQ(*doc.find_title(), paragraph("Second").set_style(builtin::title));
    EXPECT_TRUE(doc.meta.title.empty());
}

TEST(BASIC_USAGE, FindTitleAnywhere)
{
    // A title placed by hand, after a logo, is still the title.
    text_doc doc{paragraph("Logo"), paragraph("Report").set_style(builtin::title)};
    doc.set_title("Renamed");
    EXPECT_EQ(doc.body.size(), 2U);
    EXPECT_EQ(doc.body.back(), to_block(paragraph("Renamed").set_style(builtin::title)));
}
