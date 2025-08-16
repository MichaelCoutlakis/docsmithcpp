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
#include <string>

#include "docsmithcpp/nodes.h"

namespace docsmith
{

class text : public element_base<text>, public elem_tagged<text, elem_t::txt>
{
public:
    explicit text(const char *s) :
        m_text(s)
    {
    }

    explicit text(const std::string &s) :
        m_text(s)
    {
    }

    bool operator==(const text &other) const { return m_text == other.m_text; }

    std::string m_text;
};
}
