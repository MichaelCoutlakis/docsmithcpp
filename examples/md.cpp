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

// Markdown in and out through the document model:
//
//   example_md                    build a small report and save report.md
//   example_md in.odt [out.md]    convert an .odt to Markdown
//   example_md in.md [out]        convert Markdown to out.odt and out.html
//                                 (needs DOCSMITHCPP_MARKDOWN)

#include <filesystem>
#include <iostream>

#include <docsmithcpp/docsmith.h>

using namespace docsmith;
namespace fs = std::filesystem;

namespace
{
text_doc report()
{
    text_doc doc;
    const auto pass = doc.styles().insert(text_style("Pass", text_props(font_weight::bold))).first;
    const auto steps = doc.styles().insert(list_style("Steps", {number_level{.level = 1}})).first;

    table results(3, 2);
    results.set_header_rows(1);
    results.row(0).assign("Test", "Result");
    results.row(1).assign("parse", paragraph(span("pass").set_style(pass)));
    results.row(2).assign("write", paragraph(span("pass").set_style(pass)));

    doc.set_title("Nightly test report");
    doc.add(heading(1, bookmark{"summary"}, "Summary"));
    doc.add(paragraph("All ",
        span("2").set_style(pass),
        " tests passed. Build with ",
        span("cmake --build").set_style(builtin::source_text),
        ", see ",
        hyperlink("https://github.com/MichaelCoutlakis/docsmithcpp", "the project"),
        "."));
    doc.add(results);
    doc.add(heading(1, "Steps"));
    doc.add(list(list_item("Configure"), list_item("Build"), list_item("Test")).set_style(steps));
    doc.add(paragraph("[ctest] 100% tests passed, 0 tests failed out of 2")
            .set_style(builtin::preformatted));
    doc.add(paragraph("Back to the ", hyperlink(bookmark{"summary"}, "summary"), "."));
    return doc;
}
} // namespace

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        md::file(report()).save("report.md");
        std::cout << "Wrote report.md\n";
        return 0;
    }

    const fs::path in = argv[1];
    if(in.extension() == ".md")
    {
#ifdef DOCSMITHCPP_MARKDOWN
        const fs::path out = argc > 2 ? fs::path(argv[2]) : fs::path(in).replace_extension();
        const auto f = md::file::load(in);
        odt::file(f.doc).save(fs::path(out).replace_extension(".odt"));
        html::file(f.doc).save(fs::path(out).replace_extension(".html"));
        std::cout << "Wrote " << fs::path(out).replace_extension(".odt").string() << " and .html\n";
        return 0;
#else
        std::cerr << "Markdown input needs DocSmith++ built with DOCSMITHCPP_MARKDOWN\n";
        return 1;
#endif
    }

    const fs::path out = argc > 2 ? fs::path(argv[2]) : fs::path(in).replace_extension(".md");
    md::file(odt::file::load(in).doc).save(out);
    std::cout << "Wrote " << out.string() << "\n";
    return 0;
}
