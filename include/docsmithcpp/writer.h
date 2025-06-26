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
#include <iostream>
#include <variant>

#include "text_doc.h"

namespace docsmith
{
class writer
{
public:
    virtual void write(const hyperlink &link) { }
    virtual void write(const span &s) { }
    virtual void write(const TextDoc &doc) { }
    virtual void write(const list_item &item) { }
    virtual void write(const list &list) { }
    virtual void write(const paragraph &p) { }
    virtual void write(const Figure &figure) { }
    virtual void write(const heading &h) { }
    virtual void write(const Table &t) { }
    virtual void write(const Cell &c) { }

    template<typename T>
    void write(const T& 

    //// struct WriteVisitor
    ////{
    ////     template<typename WriterType, typename TypeToWrite>
    ////     void operator()(WriterType& writer_object, TypeToWrite type_to_write)
    ////     {
    ////         //writer_object.Write(type_to_write);
    ////     }
    //// };
    //template <typename TypeToWrite>
    //void WriteVariant(TypeToWrite t)
    //{
    //    // std::visit(WriteVisitor{}, *this, t);
    //    std::visit([this](const auto &var) { Write(var); }, t);
    //}

    //template <typename Parent>
    //void WriteChildren(const Parent &parent)
    //{
    //    for (const auto &child : parent.Items())
    //        WriteVariant(child);
    //}

private:
};
}