/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/

// Fill in a template with replace() and insert_before() / insert_after():
// loads examples/report_template.odt and writes
// "Example Filled-in Template.odt" and ".html".
//
//   example_fill_template

#include <iostream>

#include "docsmithcpp/docsmith.h"

int main()
{
    using namespace docsmith;

    auto f = odt::file::load(DOCSMITHCPP_EXAMPLES_DIR "/report_template.odt");
    f.doc.meta.title = "Filled-in Template";

    // Each returns false if the template has no such placeholder.
    bool filled = true;
    // By the placeholder's text: the paragraph "{{summary}}" is replaced.
    filled &= f.doc.replace(text{"{{summary}}"},
        {paragraph("All tests passed."), list(list_item("build"), list_item("unit tests"))});
    // By a bookmark: content goes before and after the paragraph holding it.
    filled &= f.doc.insert_before(bookmark{"details"}, paragraph("Inserted before the details."));
    filled &= f.doc.insert_after(bookmark{"details"}, paragraph("Inserted after the details."));
    // By a style, here the template's footer paragraph (in the page footer).
    filled &= f.doc.insert_after(
        para_style_name("Footer"), paragraph("Confidential").set_style("Footer"));
    if(!filled)
    {
        std::cerr << "The template is missing a placeholder\n";
        return 1;
    }

    f.save("Example Filled-in Template.odt");
    html::file(f.doc).save("Example Filled-in Template.html");
    std::cout << "Wrote Example Filled-in Template.odt and .html\n";
    return 0;
}
