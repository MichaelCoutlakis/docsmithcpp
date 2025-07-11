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
#include <stack>

#include "docsmithcpp/element.h"
#include <pugixml.hpp>

namespace docsmith::odt
{
class xml_writer : public element_visitor
{
public:
    xml_writer();

    pugi::xml_node get_xml() { return m_node_stack.top(); }

    void visit(const class text &) override;
    void visit(const class span &) override;

    void visit(const class heading &) override;
    void visit(const class paragraph &) override;

    void visit(const class hyperlink &) override;
    void visit(const class text_doc &) override;

    void visit(const class list &) override;
    void visit(const class list_item &) override;
    void visit(const class list_style_num &) override;
    void visit(const class list_style_bullet &) override;

    void visit(const class frame &) override;
    void visit(const class image &) override;

    void push() override;
    void pop() override;

private:
    pugi::xml_document m_doc; //!< This object owns all node memory
    std::stack<pugi::xml_node> m_node_stack;

    pugi::xml_node m_current;
    pugi::xml_node get_current();
};
}
