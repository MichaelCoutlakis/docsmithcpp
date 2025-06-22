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
#include "docsmithcpp/text_doc.h"
#include <format>
#include <ostream>

namespace docsmith

{
class io_writer : public element_visitor
{
public:
    explicit io_writer(std::ostream &os) :
        m_os(os)
    {
    }
    void visit(const text_doc &d) override
    {
        print_line("Text Doc:\n");
        m_indent += 2;
        for (auto &item : d.m_children)
            item->accept(*this);
        m_indent -= 2;
    }
    void visit(const heading &h) override
    {
        print_line("Heading: level {}\n", h.level());
        m_indent += 2;
        for (auto &item : h.m_children)
            item->accept(*this);
        m_indent -= 2;
    }
    void visit(const paragraph &p) override
    {
        print_line("paragraph\n");
        m_indent += 2;
        for (auto &item : p.m_children)
            item->accept(*this);
        m_indent -= 2;
    }
    void visit(const span &s) override { print_line("span: {}\n", s.get_text()); }

    template <typename Unhandled>
    void operator()(const Unhandled &unhandled)
    {
        m_os << "Unhandled: " << typeid(unhandled).name() << "\n";
    }

private:
    std::ostream &m_os;
    int m_indent{0};

    template <typename... Args>
    void print_line(std::format_string<Args...> fmt, Args &&...args)
    {
        m_os << get_indent() << std::format(fmt, std::forward<Args>(args)...);
    }
    std::string get_indent() const { return std::string(m_indent, ' '); }
};
}