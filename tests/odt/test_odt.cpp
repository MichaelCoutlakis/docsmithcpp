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
using par = paragraph;

template <typename Elem1, typename Elem2>
void print_compare(const element_base<Elem1> &expected, const element_base<Elem2> &actual)
{
    io_writer w(std::cout);
    std::cout << "===============\nExpected:\n===============\n";
    expected.accept(w);
    std::cout << "===============\nActual:  \n===============\n";
    actual.accept(w);
}

bool do_user_verification(std::string request)
{
    std::cout << request << "\n";
    std::cout << "Enter (pass|fail):";

    for(;;)
    {
        std::string result;
        std::cin >> result;
        if(result == "pass")
            return true;
        if(result == "fail")
            return false;
        std::cout << "Unexpected response: " << result << ". Try again\n";
    }
}

void open_file(std::string filename)
{
    std::string cmd = R"("start "" ")" + filename + R"(")";
    std::system(cmd.c_str());
}

TEST(DOCSMITH, BasicUsage)
{
    heading h{1, text{"This is the heading"}};
    par p{text{"Some Paragraph"}, text{"some other span"}};

    auto h_copy = h;
    text_doc d{h_copy, p};

    return;
}

TEST(ODT, ParseBasicFile)
{
    auto f = odt_file("odt/basic.odt");
    const text_doc actual = f.parse_text_doc();
    const text_doc expected{heading{1, "Heading 1"}, par{"This is the first par."}};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseModerateFile)
{
    auto f = odt_file("odt/moderate.odt");
    text_doc actual = f.parse_text_doc();

    // Remove paragraph style names:
    auto v = actual.find_all<styled_base>([](element *p) { return p->is_type(elem_t::p); });
    for(auto &e : v)
        e->style(style_name{});

    list_style arabic_from7("L1", {list_style_num(style{}, 1, list_enum::arabic, ")", 7)});
    list_style bullets("L2", {list_style_bullet(style{}, 1, bullet_type::bullet())});
    list_style alpha("L3", {list_style_num(style{}, 1, list_enum::lower_alpha, ")", 3)});
    list_style bullets2("L4", {list_style_bullet(style{}, 1, bullet_type::bullet())});

    text_doc expected{//
        heading{1, "Heading 1"},
        par{"First paragraph"},
        heading{2, "Second Heading"},
        par{"Second paragraph."},
        list{list_item{par{"First numbered list item, numbering starts at 7."}},
            list_item{par{"Item two."}}}
            .set_style(arabic_from7),
        par{"Bullet points:"},
        list{list_item{par{"First; and"}}, list_item{par{"Second."}}}.set_style(bullets),
        par{"Alpha list:"},
        list{list_item{par{"Item a"}}}.set_style(alpha),
        list{list_item{list{list_item{par{"Sub items of alpha list are bullets"}}}}}.set_style(
            bullets2),
        list{list_item{par{"Item b"}}}.set_style(alpha),
        par{text{"Here is a url to "},
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
    style im_style("fr1", graphics_props{});
    text_doc expected{par{
        frame{image{"Pictures/100000000000002A0000002A13903900F3783A21.png"}}.set_style(im_style)}
                          .set_style("Standard")};

    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseParagraphStyle)
{
    auto f = odt_file("odt/paragraph_with_style.odt");
    const text_doc actual = f.parse_text_doc();
    text_doc expected{par{text{"Paragraph with style."}}.set_style("Text_20_body")};

    bool b = expected == actual;
    print_compare(expected, actual);
    EXPECT_EQ(expected, actual);
}

TEST(ODT, ParseComplexFile)
{
    // auto f = odt_file("odt/complex.odt");
    // const text_doc actual = f.parse_text_doc();

    // list_style_num arabic_list_from7(style_name("L1"), list_enum::arabic, ")", 7);
    // list_style_bullet bullets(style_name("L2"), bullet_type::bullet());
    // list_style_num alpha(style_name("L3"), list_enum::lower_alpha, ")", 3);
    // list_style_bullet bullets2(style_name("L4"), bullet_type::bullet());

    // const text_doc expected{//
    //     heading{1, "Heading 1"},
    //     par{"First par"},
    //     heading{2, "Second Heading"},
    //     par{"Second par."},
    //     list{list_item{par{"First numbered list item, numbering starts at 7."}},
    //         list_item{par{"Item two."}}}
    //         .set_style(arabic_list_from7),
    //     par{"Bullet points:"},
    //     list{list_item{par{"First; and"}}, list_item{par{"Second."}}}.set_style(bullets),
    //     par{"Alpha list:"},
    //     list{list_item{par{"Item a"}}}.set_style(alpha),
    //     list{list_item{list{list_item{par{"Sub items of alpha list are bullets "}}}}},
    //     list{list_item{par{"Item b"}}}.set_style(alpha),
    //     par{text{"Here is a url to "},
    //         hyperlink{"https://github.com/MichaelCoutlakis/docsmithcpp", "docsmith"},
    //         text{" on "},
    //         span{span{text{"GitHub"}}.set_style("T1")}.set_style("Source_20_Text"),
    //         text{"."}}};

    // print_compare(expected, actual);
    // EXPECT_EQ(expected, actual);
}

TEST(ODT, WriteLists)
{
    style big_text("BigStyle", text_props(font_size(48)));

    list_style arabic("L0", {list_style_num(style{}, 1, list_enum::arabic, ".")});
    list_style arabic_from7("L1", {list_style_num(big_text, 1, list_enum::arabic, ")", 7)});
    list_style bullets("L2", {list_style_bullet(style{}, 1, bullet_type::bullet())});
    list_style alpha("L3", {list_style_num(style{}, 1, list_enum::lower_alpha, ")", 3)});

    text_doc d{
        list{list_item{"First point"}, list_item{"Second point"}}.set_style(bullets),
        list{list_item{"Number 1"}, list_item{par{"Number 2"}.set_style(big_text)}}.set_style(
            arabic),
        list{list_item{"Number 7"}, list_item{"Number 8"}}.set_style(arabic_from7),
        list{list_item{"Item a"}, list_item{"Item b"}}.set_style(alpha) //
    };

    d.styles().add(big_text);
    d.list_styles().add(arabic);
    d.list_styles().add(arabic_from7);
    d.list_styles().add(bullets);
    d.list_styles().add(alpha);
    odt_file f("odt/out/write_lists.odt");
    f.save(d);

    io_writer w(std::cout);
    d.accept(w);

    // EXPECT_TRUE(do_user_verification("Open write_lists.odt and confirm the following:"));

    open_file(f.filename());
}

TEST(ODT, GenerateBookmark)
{
    text_doc d;
    style style_page_break("PageBreak", paragraph_props(break_before{break_type::page}));

    d.add(paragraph("Some text"));
    bookmark b("Bookmark 1");
    d.add(paragraph{"paragraph with bookmark", b});
    d.add(paragraph{"Next paragraph with page break to ensure scrolling is necessary when bookmark "
                    "link is followed"}
              .set_style(style_page_break));

    d.styles().add(style_page_break);

    d.add(paragraph(hyperlink("#Bookmark 1", "Link back to the bookmark")));

    odt_file f("odt/out/bookmark.odt");
    f.save(d);
    open_file(f.filename());
}