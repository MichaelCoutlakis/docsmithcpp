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
#include "docsmithcpp/odt/ODT_File.h"

using namespace docsmith;

TEST(ODT, ParseBasicFile)
{
    auto odt_file = docsmith::ODT_File("odt/basic.odt");
    auto actual = odt_file.parse_text_doc();
    const TextDoc expected{ Heading{1, "Heading 1" }, Paragraph{ "This is the first paragraph." } };
    EXPECT_EQ(expected, actual);
}
