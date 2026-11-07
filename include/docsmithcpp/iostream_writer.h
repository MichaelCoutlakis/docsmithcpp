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
#include <optional>
#include <ostream>
#include <string>
#include <variant>

#include "docsmithcpp/text_doc.h"

namespace docsmith
{
/// A debug printer for the document tree. It is just a set of operator()
/// overloads driven by std::visit -- no element_visitor base class, and the
/// node types know nothing about it.
class io_writer
{
public:
    explicit io_writer(std::ostream &os) :
        m_os(os)
    {
    }

    void operator()(const text_doc &d)
    {
        line("Text Doc:");
        ++m_indent;
        for(const auto &b : d.body)
            (*this)(b);
        --m_indent;
    }

    void operator()(const block_node &b) { std::visit(*this, b.v); }
    void operator()(const inline_node &i) { std::visit(*this, i.v); }

    void operator()(const paragraph &p)
    {
        line("Paragraph: style=", p.style.str());
        kids(p.items());
    }
    void operator()(const heading &h)
    {
        line("Heading: level ", h.level);
        kids(h.items());
    }
    void operator()(const span &s)
    {
        line("Span: style=", s.style.str());
        kids(s.items());
    }
    void operator()(const hyperlink &h)
    {
        const std::string style = h.style.empty() ? "" : " style=" + h.style.str();
        std::visit(overloaded{[&](const url_target &u) { line("Hyperlink: ", u.url, style); },
                       [&](const bookmark_target &b) { line("Hyperlink: bookmark ", b.name, style); }},
            h.target);
        kids(h.items());
    }
    void operator()(const text &t) { line("Text: ", t.content); }
    void operator()(const frame &f)
    {
        auto pts = [](const std::optional<length> &l)
        { return l ? std::to_string(l->m_points) + "pt" : std::string("auto"); };
        line("Frame: style=", f.style.str(), ", size=", pts(f.width), " x ", pts(f.height));
        ++m_indent;
        (*this)(f.img);
        --m_indent;
    }
    void operator()(const image &i) { line("Image: uri=", i.uri); }
    void operator()(const bookmark &b) { line("Bookmark: name=", b.name); }
    void operator()(const field &f)
    {
        constexpr const char *names[] = {"page number", "page count", "date", "title"};
        line("Field: ", names[static_cast<int>(f.type)]);
    }
    void operator()(const page_break &) { line("Page Break"); }
    void operator()(const table_of_contents &t)
    { line("Table of Contents: title=", t.title, ", levels=", t.levels); }
    void operator()(const unknown &) { line("Unknown (pass-through)"); }

    void operator()(const list &l)
    {
        line("List: style ", l.style.str(), l.continue_numbering ? ", continues" : "");
        ++m_indent;
        for(const auto &it : l.items())
            (*this)(it);
        --m_indent;
    }
    void operator()(const list_item &it)
    {
        line(it.numbered ? "List Item:" : "List Item: unnumbered",
            it.start_value ? " from " + std::to_string(*it.start_value) : std::string());
        ++m_indent;
        for(const auto &b : it.items())
            (*this)(b);
        --m_indent;
    }

    void operator()(const table &t)
    {
        line("Table: (", t.num_rows(), "x", t.num_cols(), "), header rows=", t.header_rows());
        ++m_indent;
        for(size_t r = 0; r != t.num_rows(); ++r)
            for(size_t c = 0; c != t.num_cols(); ++c)
                (*this)(t.cell(r, c));
        --m_indent;
    }
    void operator()(const table_cell &c)
    {
        line("Cell: rowspan=", c.rowspan, ", colspan=", c.colspan, ", style=", c.style.str());
        ++m_indent;
        for(const auto &b : c.items())
            (*this)(b);
        --m_indent;
    }

private:
    template <typename Kids>
    void kids(const Kids &ks)
    {
        ++m_indent;
        for(const auto &k : ks)
            (*this)(k);
        --m_indent;
    }

    /// Writes the indent, then each of parts, then a newline.
    template <typename... Parts>
    void line(const Parts &...parts)
    {
        m_os << std::string(m_indent * 2, ' ');
        (m_os << ... << parts);
        m_os << '\n';
    }

    std::ostream &m_os;
    int m_indent{0};
};
} // namespace docsmith
