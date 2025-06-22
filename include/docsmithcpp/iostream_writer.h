///******************************************************************************
// * Copyright 2025 Michael Coutlakis
// *
// * SPDX-License-Identifier: Apache-2.0
// *
// * Licensed under the Apache License, Version 2.0 (the "License");
// * you may not use this file except in compliance with the License.
// * You may obtain a copy of the License at
// *
// *     http://www.apache.org/licenses/LICENSE-2.0
// *
// * Unless required by applicable law or agreed to in writing, software
// * distributed under the License is distributed on an "AS IS" BASIS,
// * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// * See the License for the specific language governing permissions and
// * limitations under the License.
// *****************************************************************************/
//#pragma once
//#include "docsmithcpp/text_doc.h"
//#include <ostream>
//
//namespace docsmith
//
//{
//class io_writer
//{
//public:
//    explicit io_writer(std::ostream &os)
//        : m_os(os)
//    {
//    }
//    void operator()(const text_doc &d)
//    {
//        m_os << "Text Doc:\n";
//        for (auto &item : d.items())
//            std::visit(*this, item);
//    }
//    void operator()(const heading &h)
//    {
//        m_os << "heading (level " << h.Level() << "): ";
//        for (auto &item : h.Items())
//            std::visit(*this, item);
//    }
//    void operator()(const paragraph &p)
//    {
//        m_os << "paragraph: ";
//        for (auto &item : p.Items())
//            std::visit(*this, item);
//    }
//    void operator()(const hyperlink &url)
//    {
//        m_os << "Hyperlink: " << url.GetURL() << "\n";
//    }
//    void operator()(const span &s) { m_os << "span: " << s.GetText() << "\n"; }
//    void operator()(const std::string &s) { m_os << s << "\n"; }
//
//    //void operator()(const list &l)
//    //{
//    //    m_os << "list: \n";
//    //    for (auto &item : l.Items())
//    //        // std::visit(*this, item);
//    //        this->operator()(item);
//    //}
//
//    template <typename Unhandled>
//    void operator()(const Unhandled &unhandled)
//    {
//        m_os << "Unhandled: " << typeid(unhandled).name() << "\n";
//    }
//
//    void write(const text_doc &d) { this->operator()(d); }
//
//private:
//    std::ostream &m_os;
//    int m_indent{0};
//};
//}