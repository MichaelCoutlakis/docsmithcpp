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

#include <string>

#include <gtest/gtest.h>

#include "zip/archive.h"

using namespace docsmith;

TEST(ZIP, RoundTripKeepsOrderAndMethod)
{
    const std::string big(10000, 'x');

    zip::writer w;
    w.add("b.txt", big);
    w.add("dir/", {});
    w.add("a.txt", "stored", zip::method::store);
    const std::string bytes = w.finish();

    const auto entries = zip::read(bytes);
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].name, "b.txt");
    EXPECT_EQ(entries[0].data, big);
    EXPECT_EQ(entries[0].m, zip::method::deflate);
    EXPECT_EQ(entries[1].name, "dir/");
    EXPECT_TRUE(entries[1].data.empty());
    EXPECT_EQ(entries[2].name, "a.txt");
    EXPECT_EQ(entries[2].data, "stored");
    EXPECT_EQ(entries[2].m, zip::method::store);

    // Deflate actually compressed the repetitive entry.
    EXPECT_LT(bytes.size(), big.size());
}

TEST(ZIP, EmptyArchive)
{
    zip::writer w;
    EXPECT_TRUE(zip::read(w.finish()).empty());
}

TEST(ZIP, NotAnArchiveThrows) { EXPECT_THROW(zip::read("not a zip file"), std::runtime_error); }
