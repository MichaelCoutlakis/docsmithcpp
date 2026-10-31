/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/

// Build a document in code and save it as an .odt file.
//
//   example_create_basic_odt [output.odt]

#include <iostream>

#include "docsmithcpp/docsmith.h"

int main(int argc, char **argv)
{
    using namespace docsmith;

    const std::string out = argc > 1 ? argv[1] : "basic.odt";

    text_doc doc{
        heading(1, "Hello World!"),
        paragraph("This is the standard example, with a ",
            hyperlink("https://github.com/MichaelCoutlakis/docsmithcpp", "link"), "."),
        heading(2, "A list"),
        list(list_item("First point"), list_item("Second point")),
        heading(2, "A table"),
        table::from_cells({
            {table_cell(paragraph("Name")), table_cell(paragraph("Value"))},
            {table_cell(paragraph("answer")), table_cell(paragraph("42"))},
        }),
    };

    odt::file(doc).save(out);
    std::cout << "Wrote " << out << "\n";
    return 0;
}
