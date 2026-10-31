/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/

// Build a document in code and save it as .odt and .html.
//
//   example_create_document [name]     (writes <name>.odt and <name>.html)

#include <iostream>

#include "docsmithcpp/docsmith.h"

int main(int argc, char **argv)
{
    using namespace docsmith;

    const std::string name = argc > 1 ? argv[1] : "basic";

    style note("Note", text_props(font_style::italic));
    list_style steps("Steps", {list_style_num(style{}, 1, list_enum::arabic)});

    text_doc doc{
        heading(1, "Hello World!"),
        paragraph("This is the standard example, with a ",
            hyperlink("https://github.com/MichaelCoutlakis/docsmithcpp", "link"), "."),
        paragraph("Styles carry over to both formats.").set_style(note),
        heading(2, "A list"),
        list(list_item("Build the document"), list_item("Save it")).set_style(steps),
        heading(2, "A table"),
        table::from_cells({
            {table_cell(paragraph("Name")), table_cell(paragraph("Value"))},
            {table_cell(paragraph("answer")), table_cell(paragraph("42"))},
        }),
    };
    doc.styles().add(note);
    doc.list_styles().add(steps);

    odt::file(doc).save(name + ".odt");

    html::file page(doc);
    page.css = "body { font-family: sans-serif; max-width: 40em; margin: auto; }\n"
               "table { border-collapse: collapse; } td { border: 1px solid #999; padding: 0.3em; }\n";
    page.save(name + ".html");

    std::cout << "Wrote " << name << ".odt and " << name << ".html\n";
    return 0;
}
