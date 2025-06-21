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
#include <iostream>
#include <variant>

#include "text_doc.h"

namespace docsmith
{

using BlockType = std::variant<text_doc , Heading, Paragraph, List, ListItem, Figure, Span, std::string>;
//using SpanType = std::variant<Figure, HyperLink, Span>;

template<class, class, class = void>
struct has_add_member : std::false_type{};

template<class Parent, class TypeToAdd>
struct has_add_member<Parent, TypeToAdd, std::void_t<decltype(std::declval<Parent>().add(std::declval<TypeToAdd>()))>> : public std::true_type{};

template<typename ThisItemType, typename TypeToAdd>
void add_item(ThisItemType& this_item, TypeToAdd item_to_add)
{
    if constexpr(has_add_member<ThisItemType, TypeToAdd>::value)
        this_item.add(item_to_add);
    else
        std::cout << "Item " << typeid(this_item).name() << " has no add function for " << typeid(item_to_add).name() << ", skipping\n";
}

struct AddItemVisitor
{
    template<typename ThisItemType, typename TypeToAdd>
    void operator()(ThisItemType& this_item, TypeToAdd item_to_add)
    {
        add_item(this_item, item_to_add);
    }
};
}