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

#include <gtest/gtest.h>

#include "docsmithcpp/element.h"
#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/text_doc.h"

using namespace docsmith;

TEST(DOCSMITH, BasicUsage)
{
    heading h{1, span{"This is the heading"}};
    paragraph p{span{"Some Paragraph"}, span{"some other span"}};

    auto h_copy = h;
    text_doc d{h_copy, p};

    return;
}

TEST(ODT, ParseBasicFile)
{
    auto f = odt_file("odt/basic.odt");
    const text_doc actual = f.parse_text_doc();
    const text_doc expected{
        heading{1, span{"Heading 1"}}, paragraph{span{"This is the first paragraph."}}};

    std::cout << "===============\n";
    io_writer w(std::cout);
    w.visit(expected);
    std::cout << "===============\n";
    w.visit(actual);
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(expected, actual);
}

// TEST(ODT, ParseModerateFile)
//{
//     auto f = odt_file("odt/moderate.odt");
//     const text_doc actual = f.parse_text_doc();
//     const text_doc expected{heading{1, "heading 1"},
//         paragraph{"First paragraph"}, heading{2, "Second heading"},
//         paragraph{"Second paragraph."}
//        /* list{list::Marker::DecimalNum, 1, list_item{"Numbered list item one."},
//             list_item{"Item two."}}*/
//     };
//
//     std::cout << "===============\n";
//     io_writer w(std::cout);
//     w.write(expected);
//     std::cout << "===============\n";
//     w.write(actual);
//     EXPECT_EQ(expected, actual);
// }