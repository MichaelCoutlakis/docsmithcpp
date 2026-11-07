/******************************************************************************
 * Copyright 2026 Michael Coutlakis
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

#include <exception>
#include <string>
#include <utility>

namespace docsmith
{

/// What went wrong, for an error.
enum class error_code
{
    io,             //!< A file couldn't be read or written
    not_a_document, //!< The file isn't of the format asked for (e.g. an .odt to docx::file::load)
    malformed,      //!< The file is of the format, but damaged or unreadable
    missing_image,  //!< An image's uri names no resource and no file on disk
    unsafe_path,    //!< A resource name would be written outside its folder
};

/// What the library throws when reading or writing a document fails. Misuse
/// of the model (a cell span under 1, an index past the end) throws
/// std::invalid_argument or std::out_of_range instead.
///
///     try { auto f = docx::file::load(p); }
///     catch(const docsmith::error &e) { if(e.code() == error_code::not_a_document) ... }
class error : public std::exception
{
public:
    error(error_code code, std::string message) :
        m_code(code), m_message(std::move(message))
    {
    }

    error_code code() const noexcept { return m_code; }
    const char *what() const noexcept override { return m_message.c_str(); }

private:
    error_code m_code;
    std::string m_message;
};

} // namespace docsmith
