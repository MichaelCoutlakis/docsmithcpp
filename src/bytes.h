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
#pragma once

// Private: whole files as bytes. std::filesystem::path keeps non-ASCII names
// intact on Windows, where path::string() would give the ANSI code page.

#include <filesystem>
#include <string>
#include <string_view>

namespace docsmith::detail
{

std::string read_bytes(const std::filesystem::path &p);

/// Write the file, creating its parent directories.
void write_bytes(const std::filesystem::path &p, std::string_view bytes);

/// The path as UTF-8, for messages.
std::string utf8(const std::filesystem::path &p);

} // namespace docsmith::detail
