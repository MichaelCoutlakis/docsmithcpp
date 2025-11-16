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

#include <type_traits>

#include "docsmithcpp/forward_decl.h"

namespace docsmith
{
// Helper to define allowed relationships between elements:
template <typename Parent, typename Child>
struct is_valid_child : std::false_type
{
};

template <typename Parent, typename Child>
constexpr bool is_valid_child_v = is_valid_child<Parent, Child>::value;

template <typename ThisType, typename TestType>
using enable_if_not_same_t =
    std::enable_if_t<!std::is_same_v<std::decay_t<ThisType>, std::decay_t<TestType>>>;

// clang-format off

// bookmark parents
template <> struct is_valid_child<heading, bookmark> : std::true_type {};
template <> struct is_valid_child<paragraph, bookmark> : std::true_type {};
template <> struct is_valid_child<span, bookmark> : std::true_type {};

// span children
template <> struct is_valid_child<span, span> : std::true_type {};
template <> struct is_valid_child<span, text> : std::true_type {};

// hyperlink children
template <> struct is_valid_child<hyperlink, span> : std::true_type {};
template <> struct is_valid_child<hyperlink, text> : std::true_type {};

// frame children
template <> struct is_valid_child<frame, image> : std::true_type {};

// paragraph children
template <> struct is_valid_child<paragraph, span> : std::true_type {};
template <> struct is_valid_child<paragraph, text> : std::true_type {};
template <> struct is_valid_child<paragraph, hyperlink> : std::true_type {};
template <> struct is_valid_child<paragraph, frame> : std::true_type {};

// heading children
template <> struct is_valid_child<heading, paragraph> : std::true_type {};
template <> struct is_valid_child<heading, span> : std::true_type {};
template <> struct is_valid_child<heading, text> : std::true_type {};

// text document children
template <> struct is_valid_child<text_doc, heading> : std::true_type {};
template <> struct is_valid_child<text_doc, paragraph> : std::true_type {};
template <> struct is_valid_child<text_doc, list> : std::true_type {};
template <> struct is_valid_child<text_doc, frame> : std::true_type {};
template <> struct is_valid_child<text_doc, table> : std::true_type {};

// list and list related children
template <> struct is_valid_child<list_style, list_style_bullet> : std::true_type {};
template <> struct is_valid_child<list_style, list_style_num> : std::true_type {};

template <> struct is_valid_child<list_item, paragraph> : std::true_type {};
template <> struct is_valid_child<list_item, heading> : std::true_type {};
template <> struct is_valid_child<list_item, list> : std::true_type {};

template <> struct is_valid_child<list, list_item> : std::true_type {};

// table cell children
template <> struct is_valid_child<table_cell, text> : std::true_type {};
template <> struct is_valid_child<table_cell, paragraph> : std::true_type {};
template <> struct is_valid_child<table_cell, table> : std::true_type {};

// table children
template <> struct is_valid_child<table, table_cell> : std::true_type {};

// clang-format on
}