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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "docsmithcpp/odt/file.h"
#include "docsmithcpp/text_doc.h"

using namespace docsmith;

namespace
{

std::string read_file(const std::filesystem::path &p)
{
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/// Little-endian 16-bit value at offset i.
unsigned u16(const std::string &s, std::size_t i)
{ return static_cast<std::uint8_t>(s[i]) | static_cast<std::uint8_t>(s[i + 1]) << 8; }

} // namespace

TEST(ODT, MimetypeIsFirstAndStored)
{
    odt::file(text_doc{paragraph("Body")}).save("odt/out/mimetype.odt");
    const std::string bytes = read_file("odt/out/mimetype.odt");

    // The first local file header, as ODF requires: mimetype, stored (method
    // 0), no data descriptor (flag bit 3) and no extra field, so the media type
    // sits at a fixed offset (38) where file-type sniffers look for it.
    const std::string type = "application/vnd.oasis.opendocument.text";
    ASSERT_GE(bytes.size(), 38 + type.size());
    EXPECT_EQ(bytes.substr(0, 4), "PK\x03\x04");
    EXPECT_EQ(u16(bytes, 6) & 0x8, 0U);
    EXPECT_EQ(u16(bytes, 8), 0U);
    EXPECT_EQ(u16(bytes, 26), 8U);
    EXPECT_EQ(u16(bytes, 28), 0U);
    EXPECT_EQ(bytes.substr(30, 8), "mimetype");
    EXPECT_EQ(bytes.substr(38, type.size()), type);
}

TEST(ODT, NonAsciiPath)
{
    const std::filesystem::path p = u8"odt/out/résumé_文件.odt";
    std::filesystem::remove(p);

    const text_doc doc{paragraph("Unicode path")};
    odt::file(doc).save(p);
    ASSERT_TRUE(std::filesystem::exists(p));
    EXPECT_EQ(odt::file::load(p).doc, doc);
}
