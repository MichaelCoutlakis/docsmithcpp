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

// Build the test report template that example_create_document fills in.
//
//   example_make_report_template [out.odt]   (default: report_template.odt)
//
// examples/report_template.odt was made by running this, then opening the
// file in LibreOffice and saving it again (or headless:
// soffice --convert-to odt --outdir <dir> report_template.odt), so that the
// template is a file as LibreOffice writes it. A template can just as well be
// made in LibreOffice by hand: the fill-in code only needs its placeholders
// and style names.

#include <iostream>

#include "docsmithcpp/docsmith.h"

int main(int argc, char **argv)
{
    using namespace docsmith;

    const std::string out = argc > 1 ? argv[1] : "report_template.odt";

    text_doc doc;
    doc.meta.title = "Test Report";

    // LibreOffice's own style names, so the template's look replaces its
    // defaults for Heading 1, Text Body and so on.
    const colour accent = colour::rgb(31, 56, 100);
    // Headings stay with the paragraph after them.
    auto heading_1 =
        doc.styles()
            .insert(paragraph_style(builtin::heading(1),
                text_props(font_name("Liberation Sans"),
                    font_size(18),
                    font_weight::bold,
                    font_colour(accent)),
                paragraph_props(space_before{pt(18)}, space_after{pt(6)}, keep_with_next{})))
            .first;
    // For the filled-in report's test case headings.
    doc.styles().insert(paragraph_style(builtin::heading(2),
        text_props(
            font_name("Liberation Sans"), font_size(14), font_weight::bold, font_colour(accent)),
        paragraph_props(space_before{pt(12)}, space_after{pt(4)}, keep_with_next{})));
    auto body = doc.styles()
                    .insert(paragraph_style("Text body",
                        text_props(font_name("Liberation Serif"), font_size(11)),
                        paragraph_props(space_after{pt(6)})))
                    .first;
    auto header = doc.styles()
                      .insert(paragraph_style("Header",
                          text_props(font_name("Liberation Sans"),
                              font_size(9),
                              font_colour(colour::rgb(89, 89, 89)))))
                      .first;
    auto footer = doc.styles()
                      .insert(paragraph_style("Footer",
                          text_props(font_name("Liberation Sans"),
                              font_size(9),
                              font_style::italic,
                              font_colour(colour::rgb(89, 89, 89)))))
                      .first;
    // Used by the filled-in report for test results.
    doc.styles().insert(
        text_style("Pass", text_props(font_weight::bold, font_colour(colour::rgb(0, 128, 0)))));
    doc.styles().insert(
        text_style("Fail", text_props(font_weight::bold, font_colour(colour::rgb(192, 0, 0)))));

    doc.page.set_header(paragraph(field{field_type::title}).set_style(header));
    doc.page.set_footer(paragraph("Generated test report \xE2\x80\x93 page ",
        field{field_type::page_number},
        " of ",
        field{field_type::page_count})
            .set_style(footer));

    doc.add(title(field{field_type::title}))
        .add(paragraph("Generated on ", field{field_type::date}, ".").set_style(body))
        .add(table_of_contents("Contents", 2))
        .add(heading(1, "Summary").set_style(heading_1))
        // Placeholders: one by its text, one by a bookmark.
        .add(paragraph("{{summary}}").set_style(body))
        .add(heading(1, "Details").set_style(heading_1))
        .add(paragraph(bookmark{"details"}, "The details of each test case go here.")
                .set_style(body));

    odt::file(doc).save(out);
    std::cout << "Wrote " << out << "\n";
    return 0;
}
