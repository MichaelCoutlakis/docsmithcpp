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

// Builds against an installed DocSmith++ (see run.cmake): a document saved as
// .odt, .html and .md, and the .odt (and, with Markdown input, the .md) read
// back.

#include <iostream>

#include <docsmithcpp/docsmith.h>

using namespace docsmith;

int main()
{
    const text_doc doc{
        heading(1, "Consumer"),
        paragraph("Built against the installed package, with a ",
            hyperlink("https://example.com", "link"),
            "."),
    };
    odt::file(doc).save("consumer.odt");
    html::file(doc).save("consumer.html");

    md::file(doc).save("consumer.md");

    if(odt::file::load("consumer.odt").doc.body != doc.body)
    {
        std::cerr << "consumer.odt did not read back as written\n";
        return 1;
    }
#ifdef DOCSMITHCPP_MARKDOWN
    if(md::file::load("consumer.md").doc.body != doc.body)
    {
        std::cerr << "consumer.md did not read back as written\n";
        return 1;
    }
#endif
    std::cout << "ok\n";
    return 0;
}
