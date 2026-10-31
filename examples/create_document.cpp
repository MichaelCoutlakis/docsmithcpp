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

    // Built in one go: nodes nest as constructor arguments.
    text_doc doc{
        heading(1, "Hello World!"),
        paragraph("This is the standard example, with a ",
            hyperlink("https://github.com/MichaelCoutlakis/docsmithcpp", "link"), "."),
        paragraph("Styles carry over to both formats.").set_style(note),
        // A link to a bookmark in a later section: "#" + the bookmark's name.
        paragraph("The results are in ", hyperlink("#results", "the table"), " below."),
        heading(2, "A list"),
        list(list_item("Build the document"), list_item("Save it")).set_style(steps),
    };
    doc.styles().add(note);
    doc.list_styles().add(steps);

    // Built piece by piece: a table is a plain value, filled in and then added.
    table results(3, 2);
    results.row(0).assign("Name", "Value");
    results.row(1).assign("answer", "42");
    results.cell(2, 0).add("pi");
    results.cell(2, 1).add(paragraph("3.14159").set_style(note));

    doc.add(heading(2, bookmark{"results"}, "A table")).add(results);

    odt::file(doc).save(name + ".odt");

    html::file page(doc);
    page.css = "body { font-family: sans-serif; max-width: 40em; margin: auto; }\n"
               "table { border-collapse: collapse; } td { border: 1px solid #999; padding: 0.3em; }\n";
    page.save(name + ".html");

    std::cout << "Wrote " << name << ".odt and " << name << ".html\n";
    return 0;
}
