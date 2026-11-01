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

// Private: the document metadata to and from docProps/core.xml and
// docProps/custom.xml.

#include "docsmithcpp/metadata.h"
#include "opc/package.h"

namespace docsmith::docx
{

metadata read_meta(const opc::package &pkg);

/// Updates the core and custom properties parts pkg holds in place, keeping
/// what the model doesn't hold, or adds them if m has something for them.
/// A new package also gets docProps/app.xml.
void write_meta(const metadata &m, opc::package &pkg, bool is_new);

} // namespace docsmith::docx
