/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/

// Build a document in code and save it as .odt, .docx and .html.
//
//   example_create_document [name]     (writes <name>.odt, <name>.docx and <name>.html)

#include <algorithm>
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
        {
            .name = "test case a",
            .passed = true,
            .detail = paragraph("some details on test case a"),
        },
        {
            .name = "test case b",
            .passed = false,
            .detail = text_doc{paragraph("some details on test case b, from ",
                                   span("ctest").set_style(builtin::source_text),
                                   ":"),
                paragraph("1/2 Test #1: a ...   Passed    0.01 sec\n"
                          "2/2 Test #2: b ...***Failed    0.02 sec")
                    .set_style(builtin::preformatted)},
        },
    };

    // The template has the styles, the header and footer (title and page
    // numbers), a table of contents and placeholders to fill in.
    auto f = odt::file::load(DOCSMITHCPP_EXAMPLES_DIR "/report_template.odt");
    text_doc &doc = f.doc;
    doc.meta.title = "DocSmith++ Test Report";
    doc.meta.author = "Example Test Application";

    // The template's Pass and Fail text styles.
    auto result = [](bool passed)
    { return paragraph(span(passed ? "Passed" : "Failed").set_style(passed ? "Pass" : "Fail")); };

    // The results table's look: lines, a shaded header and padding. ODT can't
    // keep a table style's lines in a template, so it is defined here.
    const auto heading_style = doc.add_style(
        style("Table_20_Heading", style_family::paragraph, text_props(font_weight::bold)));
    const border rule = border::solid(pt(1), colour::rgb(0x40, 0x40, 0x40));
    const border black = border::solid(pt(1), colour::rgb(0, 0, 0));
    const auto results_style = doc.add_style(style("Results",
        style_family::table,
        table_props(percent{100},
            cell_borders{rule, rule, black, black},
            inside_h{border::solid(pt(0.5), colour::rgb(0xa0, 0xa0, 0xa0))},
            inside_v{black},
            cell_padding{pt(3)},
            header_props{colour::rgb(0xe8, 0xe8, 0xe8), rule, heading_style})));

    auto summary_table = table::from_cells({{{"Test Case"}, {"Result"}}}).set_header_rows(1);
    summary_table.set_style(results_style).set_column_widths({3, 1});

    for(auto &r : records)
    {
        summary_table.add_row({
            // Test case with link to detail section
            paragraph(hyperlink(bookmark{r.name}, r.name)),
            {result(r.passed)},
        });
    }

    const auto passed =
        std::count_if(records.begin(), records.end(), [](auto &r) { return r.passed; });
    doc.replace(text{"{{summary}}"},
        {paragraph(std::to_string(passed) + " of " + std::to_string(records.size()) +
             " test cases passed.")
                .set_style("Text_20_body"),
            summary_table});

    text_doc details;
    for(auto &r : records)
    {
        // Heading with link target from summary table
        details.add(heading(2, span(r.name, bookmark{r.name})).set_style("Heading_20_2"));
        details.append(r.detail);
    }
    doc.replace(bookmark{"details"}, details);

    f.save("Example DocSmith++ Test Report.odt");
    // The same document, its styles and page included, as Word has it.
    docx::file(doc).save("Example DocSmith++ Test Report.docx");

    html::file page(doc);
    // A page has no page numbers: its footer is the template's without them.
    page.doc.page.set_footer(paragraph("Generated test report").set_style("Footer"));
    page.css = "body { max-width: 50em; margin: auto; }\n"
               "th { text-align: left; }\n"
               "header, footer { color: #595959; font-size: 9pt; }\n";
    page.save("Example DocSmith++ Test Report.html");
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
    docx::file(doc).save(name + ".docx");

    html::file page(doc);
    page.css =
        "body { font-family: sans-serif; max-width: 40em; margin: auto; }\n"
        "table { border-collapse: collapse; } td { border: 1px solid #999; padding: 0.3em; }\n";
    page.save(name + ".html");

    std::cout << "Wrote " << name << ".odt, " << name << ".docx and " << name << ".html\n";
    return 0;
}
