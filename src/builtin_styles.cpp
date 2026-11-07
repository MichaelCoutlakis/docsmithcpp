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

#include "builtin_styles.h"

#include <type_traits>

#include "docsmithcpp/visit.h"

namespace docsmith
{
namespace
{

// Arial and Courier New are on Windows and macOS, and LibreOffice substitutes
// the metric-compatible Liberation Sans and Liberation Mono where they aren't.
const font_name sans("Arial");
const font_name mono("Courier New");

/// A paragraph style based on builtin::para, so a font set there reaches it
/// where it doesn't set its own.
paragraph_style based_on_para(const para_style_name &name, paragraph_props pp, text_props tp)
{
    paragraph_style s(name, pp, tp);
    s.parent = builtin::para;
    return s;
}

style_sheet make_defaults()
{
    style_sheet out;
    const paragraph_props heading_para(space_before(pt(12)), space_after(pt(6)), keep_with_next());

    paragraph_props title_para = heading_para;
    title_para.set(text_align::centre);
    out.insert(based_on_para(
        builtin::title, title_para, text_props(sans, font_size(28), font_weight::bold)));

    constexpr int sizes[] = {18, 16, 14, 13, 12, 11, 11, 10, 10, 10};
    for(int level = 1; level <= 10; ++level)
        out.insert(based_on_para(builtin::heading(level),
            heading_para,
            text_props(sans, font_size(static_cast<float>(sizes[level - 1])), font_weight::bold)));

    out.insert(based_on_para(builtin::preformatted,
        paragraph_props(space_before(pt(0)), space_after(pt(0))),
        text_props(mono, font_size(10))));

    out.insert(text_style(builtin::source_text, text_props(mono)));
    out.insert(text_style(
        builtin::link, text_props(font_colour(colour::rgb(0x05, 0x63, 0xC1)), underline::single)));
    out.insert(text_style(builtin::emphasis, text_props(font_style::italic)));
    out.insert(text_style(builtin::strong, text_props(font_weight::bold)));
    return out;
}

} // namespace

const style_sheet &detail::builtin_defaults()
{
    static const style_sheet defaults = make_defaults();
    return defaults;
}

int detail::builtin_heading_level(const para_style_name &name)
{
    for(int level = 1; level <= 10; ++level)
        if(name == builtin::heading(level))
            return level;
    return 0;
}

detail::used_names detail::names_used(const text_doc &doc)
{
    used_names out;
    walk(doc,
        [&out](const auto &n)
        {
            using T = std::decay_t<decltype(n)>;
            if constexpr(std::is_same_v<T, paragraph>)
                out.para.insert((n.style.empty() ? builtin::para : n.style).str());
            else if constexpr(std::is_same_v<T, heading>)
                out.para.insert((n.style.empty() ? builtin::heading(n.level) : n.style).str());
            else if constexpr(std::is_same_v<T, title>)
                out.para.insert((n.style.empty() ? builtin::title : n.style).str());
            else if constexpr(std::is_same_v<T, span>)
                out.text.insert(n.style.str());
            else if constexpr(std::is_same_v<T, hyperlink>)
                out.text.insert((n.style.empty() ? builtin::link : n.style).str());
        });
    return out;
}

style_sheet detail::with_used_defaults(const style_sheet &styles, const text_doc &doc)
{
    style_sheet out = styles;
    const auto used = names_used(doc);
    for(const auto &[name, def] : builtin_defaults().para)
        if(used.para.contains(name.str()) && !out.para.contains(name))
            out.insert(def);
    for(const auto &[name, def] : builtin_defaults().text)
        if(used.text.contains(name.str()) && !out.text.contains(name))
            out.insert(def);
    return out;
}

std::optional<paragraph_style> builtin::default_style(const para_style_name &name)
{
    if(const auto *s = detail::builtin_defaults().para.find(name))
        return *s;
    return std::nullopt;
}

std::optional<text_style> builtin::default_style(const text_style_name &name)
{
    if(const auto *s = detail::builtin_defaults().text.find(name))
        return *s;
    return std::nullopt;
}

} // namespace docsmith
