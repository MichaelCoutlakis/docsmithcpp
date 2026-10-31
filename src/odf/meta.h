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

// Private: the document metadata to and from an ODF meta.xml, shared by the
// ODF formats.

#include <string>

#include "docsmithcpp/metadata.h"

namespace docsmith::odf
{
class package;

/// The metadata in a meta.xml. Dates without a time zone are read as UTC.
metadata read_meta(const std::string &xml);

/// Sets meta.xml in pkg to m. A meta.xml pkg already holds is updated in
/// place, so what the model doesn't hold (generator, statistics, editing
/// time) is kept, as are values m leaves unchanged. Without one, a meta.xml is
/// added only if m isn't empty.
void write_meta(const metadata &m, package &pkg);

} // namespace docsmith::odf
