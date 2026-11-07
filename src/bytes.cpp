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
#include "bytes.h"
#include "docsmithcpp/error.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace docsmith::detail
{
namespace fs = std::filesystem;

std::string read_bytes(const fs::path &p)
{
    std::ifstream in(p, std::ios::binary);
    if(!in)
        throw error(error_code::io, "Could not read file: " + utf8(p));
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void write_bytes(const fs::path &p, std::string_view bytes)
{
    if(p.has_parent_path())
        fs::create_directories(p.parent_path());

    std::ofstream out(p, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if(!out.flush())
        throw error(error_code::io, "Could not write file: " + utf8(p));
}

std::string utf8(const fs::path &p)
{
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

} // namespace docsmith::detail
