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

#include <gtest/gtest.h>

#include "docsmithcpp/element.h"
#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/text_doc.h"

using namespace docsmith;

#include <iostream>
#include <string>
#include <utility> // For std::move

#include <pugixml.hpp>

TEST(BASIC_USAGE, CreateDocument)
{
    text_doc doc;

    // doc.add(paragraph(text{"Hello"}, span(style_name{}, span(style_name{}, text("Hello")))));
    //paragraph p("Hello", "Goodbye");
    paragraph p;
    span s(kids{text(""), text{"nested"}});
    // span s2("", text{"nested2"});
    // s.add(s2);
    p.add(frame(image("odt/smiley.png")).set_style("fr1"));
    p.add(s);
    doc.add(p);

    std::cout << "\n\n";
    io_writer w(std::cout);
    // doc.accept(w);
    std::cout << "\n\n";

    odt_file f("odt/basic_generated.odt");
    f.save(doc);
}
