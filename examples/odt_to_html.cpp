/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/

// Convert an .odt file to HTML through the document model.
//
//   example_odt_to_html <input.odt> [output.html]
//
// Images embedded in the .odt aren't extracted yet, so they won't show.

#include <filesystem>
#include <iostream>

#include "docsmithcpp/docsmith.h"

int main(int argc, char **argv)
{
    using namespace docsmith;

    if(argc < 2)
    {
        std::cerr << "usage: " << argv[0] << " <input.odt> [output.html]\n";
        return 1;
    }
    const std::filesystem::path in = argv[1];
    const std::filesystem::path out =
        argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::path(in).replace_extension(".html");

    html::file(odt::file::load(in).doc).save(out);
    std::cout << "Wrote " << out.string() << "\n";
    return 0;
}
