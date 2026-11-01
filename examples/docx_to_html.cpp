/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/

// Convert a .docx file to HTML through the document model.
//
//   example_docx_to_html <input.docx> [output.html]
//
// The images go next to the page (html::file::resource_dir, word/media/).

#include <filesystem>
#include <iostream>

#include "docsmithcpp/docsmith.h"

int main(int argc, char **argv)
{
    using namespace docsmith;

    if(argc < 2)
    {
        std::cerr << "usage: " << argv[0] << " <input.docx> [output.html]\n";
        return 1;
    }
    const std::filesystem::path in = argv[1];
    const std::filesystem::path out =
        argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::path(in).replace_extension(".html");

    html::file(docx::file::load(in).doc).save(out);
    std::cout << "Wrote " << out.string() << "\n";
    return 0;
}
