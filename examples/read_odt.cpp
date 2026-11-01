/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/

// Parse an .odt file into the document model, print its tree and list its links.
//
//   example_read_odt <input.odt>

#include <iostream>

#include "docsmithcpp/docsmith.h"

int main(int argc, char **argv)
{
    using namespace docsmith;

    if(argc < 2)
    {
        std::cerr << "usage: " << argv[0] << " <input.odt>\n";
        return 1;
    }

    text_doc doc = odt::file::load(argv[1]).doc;

    io_writer print(std::cout);
    print(doc);

    std::cout << "\nLinks:\n";
    for(const hyperlink *a : collect<hyperlink>(doc))
        std::visit(overloaded{[](const url_target &u) { std::cout << "  " << u.url << "\n"; },
                       [](const bookmark_target &b) { std::cout << "  bookmark " << b.name << "\n"; }},
            a->target);
    return 0;
}
