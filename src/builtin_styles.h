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

#include "docsmithcpp/style.h"

namespace docsmith::detail
{

/// Every builtin's format-agnostic default (builtin::default_style()), for
/// writers to define the builtins a document uses and doesn't define.
const style_sheet &builtin_defaults();

/// n if name is builtin::heading(n), else 0. Writers give a heading style its
/// format's outline level, which the model doesn't hold.
int builtin_heading_level(const para_style_name &name);

} // namespace docsmith::detail
