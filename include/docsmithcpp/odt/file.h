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

#include <libzippp/libzippp.h>

#include "docsmithcpp/text_doc.h"

namespace docsmith
{

class odt_file
{
public:
    explicit odt_file(const std::string &filename);

    text_doc parse_text_doc();

private:
    std::string m_filename;

    libzippp::ZipArchive m_zip;
};

}