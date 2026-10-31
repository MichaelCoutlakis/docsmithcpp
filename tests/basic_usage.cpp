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

    odt_file f("odt/out/basic_generated.odt");
    f.save(doc);
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
