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
#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/odt/file.h"
#include <gtest/gtest.h>

using namespace docsmith;

TEST(ODT, ParseBasicFile)
{
    auto f = odt_file("odt/basic.odt");
    const text_doc actual = f.parse_text_doc();
    const text_doc expected{
        Heading{1, "Heading 1"}, Paragraph{"This is the first paragraph."}};
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseModerateFile)
{
    auto f = odt_file("odt/moderate.odt");
    const text_doc actual = f.parse_text_doc();
    const text_doc expected{Heading{1, "Heading 1"},
        Paragraph{"First Paragraph"}, Heading{2, "Second Heading"},
        Paragraph{"Second paragraph."}
       /* List{List::Marker::DecimalNum, 1, ListItem{"Numbered list item one."},
            ListItem{"Item two."}}*/
    };

    std::cout << "===============\n";
    io_writer w(std::cout);
    w.write(expected);
    std::cout << "===============\n";
    w.write(actual);
    EXPECT_EQ(expected, actual);
}