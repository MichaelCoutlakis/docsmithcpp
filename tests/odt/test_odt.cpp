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

template <typename Elem1, typename Elem2>
void print_compare(const element_base<Elem1> &expected, const element_base<Elem2> &actual)
{
    io_writer w(std::cout);
    std::cout << "===============\nExpected:\n===============\n";
    expected.accept(w);
    std::cout << "===============\nActual:  \n===============\n";
    actual.accept(w);
}

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
    const text_doc expected{heading{1, "Heading 1"}, paragraph{"This is the first paragraph."}};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseModerateFile)
{
    auto f = odt_file("odt/moderate.odt");
    const text_doc actual = f.parse_text_doc();

    list_style_num arabic_list_from7(style_name("L1"), list_style_num::format::arabic, ")", 7);
    list_style_bullet bullets(style_name("L2"), bullet_type::bullet());
    list_style_num alpha(style_name("L3"), list_style_num::format::lower_alpha, ")", 3);
    list_style_bullet bullets2(style_name("L4"), bullet_type::bullet());

    const text_doc expected{heading{1, "Heading 1"},
        paragraph{"First paragraph"},
        heading{2, "Second Heading"},
        paragraph{"Second paragraph."},
        list{list_item{paragraph{"First numbered list item, numbering starts at 7."}},
            list_item{paragraph{"Item two."}}}
            .set_style(arabic_list_from7.m_style_name),
        paragraph{"Bullet points:"},
        list{list_item{paragraph{"First; and"}}, list_item{paragraph{"Second."}}}.set_style(
            bullets.m_style_name),
        paragraph{"Alpha list:"},
        list{list_item{paragraph{"Item a"}}}.set_style(alpha.m_style_name),
        list{list_item{list{list_item{paragraph{"Sub items of alpha list are bullets"}}}}}
            .set_style(bullets2.m_style_name),
        list{list_item{paragraph{"Item b"}}}.set_style(alpha.m_style_name),
        paragraph{text{"Here is a url to "},
            hyperlink{"https://github.com/MichaelCoutlakis/docsmithcpp", "docsmith"},
            text{" on "},
            span{kids_tag{}, span{text{"GitHub"}}.set_style("T1")}.set_style("Source_20_Text"),
            text{"."}}};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseImage)
{
    auto f = odt_file("odt/image.odt");
    const text_doc actual = f.parse_text_doc();
    style im_style("fr1", style_graphics{});
    text_doc expected{paragraph{
        frame{image{"Pictures/100000000000002A0000002A13903900F3783A21.png"}}.set_style(im_style)}};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
}

// constexpr bool b1 = std::is_convertible_v<decltype(std::decay_t<"Hello">), std::string>;

template <typename Arg>
constexpr bool is_stringable(Arg &&arg)
{
    return std::is_convertible_v<std::decay_t<Arg>, std::string>;
}

constexpr bool b = is_stringable("hello");

TEST(ODT, ParseParagraphStyle)
{
    paragraph p1("hello");
    p1.set_style("asf");
    paragraph p2;
    p2 = p1;
    auto p3 = p1.clone();

    std::cout << "p1 style name is " << p1.get_style().get_name() << "\n";
    std::cout << "p2 style name is " << p2.get_style().get_name() << "\n";
    auto f = odt_file("odt/paragraph_with_style.odt");
    const text_doc actual = f.parse_text_doc();
    text_doc expected{paragraph{text{"Paragraph with style."}}.set_style("sdf")};
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseComplexFile)
{
    auto f = odt_file("odt/complex.odt");
    const text_doc actual = f.parse_text_doc();

    list_style_num arabic_list_from7(style_name("L1"), list_style_num::format::arabic, ")", 7);
    list_style_bullet bullets(style_name("L2"), bullet_type::bullet());
    list_style_num alpha(style_name("L3"), list_style_num::format::lower_alpha, ")", 3);
    list_style_bullet bullets2(style_name("L4"), bullet_type::bullet());

    const text_doc expected{heading{1, "Heading 1"},
        paragraph{"First paragraph"},
        heading{2, "Second Heading"},
        paragraph{"Second paragraph."},
        list{list_item{paragraph{"First numbered list item, numbering starts at 7."}},
            list_item{paragraph{"Item two."}}}
            .set_style(arabic_list_from7.m_style_name),
        paragraph{"Bullet points:"},
        list{list_item{paragraph{"First; and"}}, list_item{"Second."}}.set_style(
            bullets.m_style_name),
        paragraph{"Alpha list:"},
        list{list_item{paragraph{"Item a"}}}.set_style(alpha.m_style_name),
        list{list_item{list{list_item{paragraph{"Sub items of alpha list are bullets "}}}}},
        list{list_item{paragraph{"Item b"}}}.set_style(alpha.m_style_name),
        paragraph{text{"Here is a url to "},
            hyperlink{"https://github.com/MichaelCoutlakis/docsmithcpp", "docsmith"},
            text{" on "},
            span{span{text{"GitHub"}}.set_style("T1")}.set_style("Source_20_Text"),
            text{"."}}};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
}