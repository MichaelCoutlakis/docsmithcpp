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

void write_example_test_report()
{
    using namespace docsmith;

    struct test_case_doc
    {
        std::string name;
        bool passed;
        text_doc detail;
    };

    std::vector<test_case_doc> records{
        {.name = "test case a", .passed = true, .detail = paragraph("some details on test case a")},
        {.name = "test case b",
            .passed = false,
            .detail = paragraph("some details on test case b")}};

    text_doc doc;
    doc.meta.title = "DocSmith++ Test Report (meta data title)";
    doc.meta.author = "Example Test Application";
    doc.set_title("DocSmith++ Test Report");
    doc.add(heading(1, "Test Case Summary"));

    auto pass_style =
        doc.add_style(style("pass-style", text_props(font_weight::bold, colour::rgb(0, 128, 0))));
    auto fail_style =
        doc.add_style(style("fail-style", text_props(font_weight::bold, colour::rgb(192, 0, 0))));

    auto pass_summary = paragraph("Passed").set_style(pass_style);
    auto fail_summary = paragraph("Failed").set_style(fail_style);
    auto summary_table = table::from_cells({{{"Test Case"}, {"Result"}}});
    for(auto &r : records)
    {
        summary_table.add_row(
            // Test case with link to detail section
            {paragraph(hyperlink(bookmark{r.name}, r.name)),
                // Pass or Fail
                {r.passed ? pass_summary : fail_summary}});
    }

    doc.add(summary_table);
    doc.add(heading(1, "Test Case Detail"));

    for(auto &r : records)
    {
        // Heading with link target from summary table
        doc.add(heading(2, span(r.name, bookmark(r.name))));
        doc.append(r.detail);
    }

    if(auto t = doc.find_title(); t)
        t->add(" - Updated");

    odt::file(doc).save("Example DocSmith++ Test Report.odt");
}

int main(int argc, char **argv)
{
    write_example_test_report();

    using namespace docsmith;

    const std::string name = argc > 1 ? argv[1] : "basic";

    style note("Note", text_props(font_style::italic));
    list_style steps("Steps", {list_style_num(style{}, 1, list_enum::arabic)});

    // Built in one go: nodes nest as constructor arguments.
    text_doc doc{
        heading(1, "Hello World!"),
        paragraph("This is the standard example, with a ",
            hyperlink("https://github.com/MichaelCoutlakis/docsmithcpp", "link"),
            "."),
        paragraph("Styles carry over to both formats.").set_style(note),
        // A link to a bookmark in a later section.
        paragraph("The results are in ", hyperlink(bookmark{"results"}, "the table"), " below."),
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
    page.css =
        "body { font-family: sans-serif; max-width: 40em; margin: auto; }\n"
        "table { border-collapse: collapse; } td { border: 1px solid #999; padding: 0.3em; }\n";
    page.save(name + ".html");

    std::cout << "Wrote " << name << ".odt and " << name << ".html\n";
    return 0;
}
