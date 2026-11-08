/******************************************************************************
 * Copyright 2025 Michael Coutlakis
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#include <docsmithcpp/docsmith.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <tuple>

// Stub: a real jig talks to the unit and to its instruments.
struct test_jig
{
    std::string serial() { return "SN-1042"; }
    double output_voltage() { return 12.03; }
    double output_ripple() { return 61.7; }
    double efficiency() { return 93.4; }
};

struct requirement
{
    std::string id, param, unit;
    double min, max;
    double (test_jig::*measure)();
};

const requirement requirements[] = {
    {"PWR-001", "Output voltage", "V", 11.88, 12.12, &test_jig::output_voltage},
    {"PWR-002", "Output ripple", "mV", 0, 50, &test_jig::output_ripple},
    {"PWR-003", "Efficiency", "%", 92, 100, &test_jig::efficiency},
};

std::string num(double v)
{
    std::ostringstream s;
    s << std::fixed << std::setprecision(2) << v;
    return s.str();
}

using namespace docsmith;
const paragraph_style hdr_p(
    "header_para", text_props(font_weight::bold), paragraph_props(text_align::left));
const border line = border::solid(pt(0.5));
const table_style grid("Grid",
    table_props(table_width(percent{100}),
        header_cells(cell_props(hdr_p, background(colour::rgb(211, 238, 255)))),
        cell_borders{line, line, line, line}, inside_h{line}, inside_v{line}, cell_padding{pt(3)}));
const text_style pass("Pass", text_props(font_colour(colour::rgb(0, 0x80, 0)), font_weight::bold));
const text_style fail("Fail", text_props(font_colour(colour::rgb(0xC0, 0, 0)), font_weight::bold));
const paragraph_style hdr("Header", paragraph_props(text_align::centre),
    text_props(
        font_name("Liberation Sans"), font_size(9), font_colour(colour::rgb(151, 180, 199))));

// clang-format off
// <snippet-start:showcase>
int main()
{
    using namespace docsmith; using para = paragraph;
    // A document is a tree of plain values:
    text_doc doc{title{"DC-DC Converter Test Report"}, table_of_contents{"Contents"}};

    // Markdown is best suited for some content, with {{tricky}} replaced later
    doc.append(md::parse(R"(
# Scope

Factory acceptance test of the **DC-DC converter** at 48 V input and 5 A load,
to [ATP-0042](https://example.com/atp-0042).

# Procedure

1. Connect the unit to the test jig.
2. Soak at full load for `10 min`.
3. Measure each parameter.

# Results

{{results}}

# Sign-off

| Role   | Name | Signature | Date |
| ------ | ---- | --------- | ---- |
| Tester |      |           |      |
| QA     |      |           |      |
)"));

    // Styles defined outside main for brevity. They could also be loaded from a template.
    doc.styles().insert(grid), doc.styles().insert(pass), doc.styles().insert(fail),
    doc.styles().insert(hdr), doc.styles().insert(hdr_p);

    // The measurements come from the test program:
    test_jig jig;
    bool all_passed = true;
    auto results = table::from_cells({{"Req", "Param", "Min", "Measured", "Max", "Unit", "Result"}});
    results.set_header_rows(1);
    auto verdict = [](bool ok) { return span(ok ? "PASS" : "FAIL").set_style(ok ? pass : fail); };

    for(const requirement &r : requirements)
    {
        const double val = (jig.*r.measure)();
        const bool ok = r.min <= val && val <= r.max;
        all_passed = all_passed && ok;
        results.add_row({{r.id}, {r.param}, {num(r.min)}, {num(val)}, {num(r.max)}, {r.unit}, para{verdict(ok)}});
    }

    std::ignore = doc.replace("{{results}}",
        {para("Serial Num: ", jig.serial()), results, para("Overall: ", verdict(all_passed))});

    for(table *t : collect<table>(doc))
        t->set_style(grid);

    doc.page.set_header(para("ACME CONVERTERS").set_style(hdr));
    doc.page.set_footer(para("Page ", field{field_type::page_number}));

    odt::file(doc).save("report.odt");
    docx::file(doc).save("report.docx");
    html::save(doc, "report.html");
    md::save(doc, "report.md");
    return EXIT_SUCCESS;
}
// <snippet-end:showcase>
// clang-format on