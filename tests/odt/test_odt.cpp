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
    heading h{1, text{"This is the heading"}};
    paragraph p{text{"Some Paragraph"}, text{"some other span"}};

    auto h_copy = h;
    text_doc d{h_copy, p};

    return;
}

TEST(ODT, ParseBasicFile)
{
    auto f = odt_file("odt/basic.odt");
    const text_doc actual = f.parse_text_doc();
    const text_doc expected{
        heading{1, text{"Heading 1"}}, paragraph{text{"This is the first paragraph."}}};

    std::cout << "===============\n";
    io_writer w(std::cout);
    w.visit(expected);
    std::cout << "===============\n";
    w.visit(actual);
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseModerateFile)
{
    auto f = odt_file("odt/moderate.odt");
    const text_doc actual = f.parse_text_doc();

    list_style_num arabic_list_from7(style_name("L1"), list_style_num::format::arabic, ")", 7);
    list_style_bullet bullets(style_name("L2"), list_style_bullet::bullets::bullet());
    list_style_num alpha(style_name("L3"), list_style_num::format::lower_alpha, ")", 3);
    list_style_bullet bullets2(style_name("L4"), list_style_bullet::bullets::bullet());

    const text_doc expected{heading{1, "Heading 1"},
        paragraph{"First paragraph"},
        heading{2, "Second Heading"},
        paragraph{"Second paragraph."},
        list{arabic_list_from7.m_style_name,
            list_item{paragraph{"First numbered list item, numbering starts at 7."}},
            list_item{paragraph{"Item two."}}},
        paragraph{"Bullet points:"},
        list{bullets.m_style_name, list_item{paragraph{"First; and"}}, list_item{"Second."}},
        paragraph{"Alpha list:"},
        list{alpha.m_style_name, list_item{paragraph{"Item a"}}},
        list{bullets2.m_style_name,
            list_item{
                list{style_name{}, list_item{paragraph{"Sub items of alpha list are bullets"}}}}},
        list{alpha.m_style_name, list_item{paragraph{"Item b"}}},
        paragraph{text{"Here is a url to "},
            hyperlink{"https://github.com/MichaelCoutlakis/docsmithcpp", "docsmith"},
            text{" on "},
            span{style_name{"Source_20_Text"}, span{style_name{"T1"}, text{"GitHub"}}},
            text{"."}}};

    std::cout << "===============\n";
    io_writer w(std::cout);
    w.visit(expected);
    std::cout << "===============\n";
    w.visit(actual);
    EXPECT_EQ(expected, actual);
}