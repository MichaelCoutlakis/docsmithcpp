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
    void visit(const text_doc &d) override { print_line("Text Doc:"); }

    void visit(const heading &h) override { print_line("Heading: level {}", h.level()); }

    void visit(const paragraph &p) override { print_line("Paragraph"); }

    void visit(const span &s) override { print_line("Span:"); }

    void visit(const hyperlink &href) { print_line("Hyperlink: {}", href.get_url()); }

    void visit(const text &t) override { print_line("Text: {}", t.m_text); }

    void visit(const class list &l) override
    {
        print_line("List: Style {}", l.m_style_name.get_name());
    }

    void visit(const class list_item &li) override { print_line("List Item:"); }

    void visit(const class list_style_num &s) override
    {
        print_line("List Style Numbered: format: {} suffix: {} start from: {}",
            static_cast<char>(s.m_format),
            s.m_num_suffix,
            s.m_start_from);
    }
    void visit(const class list_style_bullet &s) override
    {
        print_line("List Style Bullet: bullet {}", s.m_bullet_char);
    }

    void visit(const class frame &f) override { print_line("Frame"); }

    void visit(const class image &i) override { print_line("Image: uri={}", i.get_uri()); }

    void push() override { m_indent += 2; }

    void pop() override { m_indent -= 2; }

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
        m_os << get_indent() << std::format(fmt, std::forward<Args>(args)...) << "\n";
    }
    std::string get_indent() const { return std::string(m_indent, ' '); }
};
}