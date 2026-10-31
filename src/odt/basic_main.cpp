/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/
#include <iostream>

#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/text_doc.h"

int main()
{
    using namespace docsmith;

    // Build a small document with the node constructors.
    text_doc doc{
        heading(1, "Hello World!"),
        paragraph("This is the standard example with a ", hyperlink("https://example.com", "link"),
            " in it."),
    };

    io_writer w(std::cout);
    w(doc);

    return 0;
}
