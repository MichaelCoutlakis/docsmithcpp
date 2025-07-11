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

#include "docsmithcpp/odt/xml_writer.h"
#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/text_doc.h"

#include <iostream>
#include <sstream>

namespace docsmith::odt
{
xml_writer::xml_writer()
{
    auto root_node = m_doc.append_child("root");
    m_node_stack.push(root_node);
    m_current = m_node_stack.top();
}

void xml_writer::visit(const text &val)
{
    get_current().append_child(pugi::node_pcdata).set_value(val.m_text);
}

void xml_writer::visit(const span &) { get_current().append_child("text:span"); }

void xml_writer::visit(const heading &) { get_current().append_child("text:h"); }

void xml_writer::visit(const paragraph &) { get_current().append_child("text:p"); }

void xml_writer::visit(const hyperlink &v)
{
    auto url = get_current().append_child("text:a");
    url.append_attribute("xlink:href") = v.get_url();
}

void xml_writer::visit(const text_doc &) { get_current().append_child("office:text"); }

void xml_writer::visit(const list &) { }

void xml_writer::visit(const list_item &) { }

void xml_writer::visit(const list_style_num &) { }

void xml_writer::visit(const list_style_bullet &) { }

void xml_writer::visit(const frame &) { get_current().append_child("draw:frame"); }

void xml_writer::visit(const image &v)
{
    auto n = get_current().append_child("draw:image");
    n.append_attribute("xlink:href").set_value(v.get_uri());
}

void xml_writer::push() { m_node_stack.push(get_current().last_child()); }

void xml_writer::pop() { m_node_stack.pop(); }

pugi::xml_node xml_writer::get_current() { return m_node_stack.top(); }
}