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
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <stdexcept>
#include <optional>
#include <list>
#include <variant>

namespace docsmith
{

class StyleName
{
public:
    StyleName(const std::string& name = {}) : m_name{ name } {}
    const std::string& GetName() const { return m_name; }

    bool operator==(const StyleName& rhs) const = default;
private:
    std::string m_name;
};

struct FontName
{
public:
    explicit FontName(const std::string& font_name) : m_font_name{ font_name } {}
private:
    std::string m_font_name;
};

enum class FontStyle
{
    Normal,
    Italic,
    Oblique
};
enum class FontWeight
{
    Normal,
    Bold
};

struct FontSize
{
    explicit FontSize(float points) : m_points{ points } {}
    float m_points;
};

class Style
{
public:
    template<typename... Args>
    Style(const StyleName& style_name, Args... args) :
        m_name{ style_name }
    {
        (Set(std::move(args)), ...);
    }

    Style& Set(const StyleName& style_name) { m_name = style_name; return *this; }
    Style& Set(const FontName& font_name) { m_font_name = font_name; return *this; }
    Style& Set(const FontSize& font_size) { m_font_size = font_size; return *this; }
    Style& Set(const FontWeight& font_weight) { m_font_weight = font_weight;return *this; }

    StyleName m_name; //!< The style name for the purpose of identification and lookup
    StyleName m_display_name{ "" }; //!< Human friendly display name

    FontName m_font_name{ "Arial" };
    FontSize m_font_size{ 12.f };
    FontStyle m_font_style{ FontStyle::Normal };
    FontWeight m_font_weight{ FontWeight::Normal };
};


class Span
{
public:
    explicit Span(const std::string& text, const StyleName& style_name = StyleName{}) : m_text{ text }, m_style_name{ style_name } {}

    Span& Set(const StyleName& style_name) { m_style_name = style_name; return *this; }
    std::string GetText() const { return m_text; }

    bool operator==(const Span& rhs) const = default;
private:
    StyleName m_style_name;
    std::string m_text;
};


class ListItem
{
public:
    ListItem() = default;
    explicit ListItem(const Span& span) : m_span{ span } {}
    explicit ListItem(const std::string& item) : m_span{ Span(item) } {}
    bool operator==(const ListItem& rhs) const = default;

    void Add(const std::string& item) { m_span.push_back(Span(item)); }
    const std::list<Span>& Items() const { return m_span; }
private:
    std::list<Span> m_span;
};

class List
{
public:
    enum class Marker
    {
        DecimalNum,
        LowerAlpha,
        UpperAlpha,
        LowerRoman,
        UpperRoman,
        Bullet
    };

    template<typename... Args>
    List(Marker marker, int start, Args&&... args) :
        m_marker(marker),
        m_start(start)
    {
        (Add(std::move(args)), ...);
    }

    void Add(std::string&& item) { m_items.emplace_back(item); }
    void Add(const std::string& item) { m_items.push_back(ListItem(item)); }
    void Add(const ListItem& item) { m_items.push_back(item); }
    void Add(ListItem&& item)
    {
        m_items.emplace_back(std::move(item));
    }

    const  std::list<ListItem>& Items() const { return m_items; }
    bool IsOrdered() const { return m_marker < Marker::Bullet; }

    int GetStart() const { return m_start; }

    bool operator==(const List& rhs) const = default;

private:
    Marker m_marker{ Marker::Bullet };
    int m_start{ 1 };
    std::list<ListItem> m_items;
};



class HyperLink
{
public:
    using Elem = std::variant<std::string, Span>;

    template<typename... Args>
    HyperLink(std::string URL, Args... args) :
        m_URL(URL)
    {
        (Add(args), ...);
    }

    std::string GetURL() const { return m_URL; }
    void Add(Elem elem)
    {
        m_content.push_back(elem);
    }
    bool operator==(const HyperLink& rhs) const = default;
private:
    std::string m_URL;
    std::list<Elem> m_content;
};

class Figure
{
public:
    explicit Figure(std::string filename) :
        m_filename(filename)
    {
    }

    std::string GetFileName() const { return m_filename; }
    bool operator==(const Figure& rhs) const
    {
        bool b = m_filename == rhs.m_filename;
        return b;
    }
private:
    std::string m_filename;
};

class Paragraph
{
public:
    /// The elements that can occur in a paragraph.
    using Elem = std::variant<std::string, Span, HyperLink, Figure, List>;

    Paragraph() = default;

    template<typename... Args>
    explicit Paragraph(Args... args)
    {
        (add(args), ...);
    }

    explicit Paragraph(Elem item) : m_items{ item } {}
    explicit Paragraph(std::initializer_list<Elem> items) : m_items{ items.begin(), items.end() } {}

    void add(const Elem& elem) { m_items.push_back(elem); }

    //Content<Elem> m_content;
    bool operator==(const Paragraph& rhs) const
    {
        bool b1 = m_style_name == rhs.m_style_name;
        bool b2 = m_items == rhs.m_items;
        return b1 && b2;
    }
    const std::list<Elem>& Items() const { return m_items; }
private:
    std::string m_style_name;
    std::list<Elem> m_items;
};

class Heading
{
public:
        /// The elements that can occur in a paragraph.
    using Elem = std::variant<std::string, Span, HyperLink>;
    //Heading(int level) : m_level(level)

    template<typename... Args>
    Heading(int level, Args... args) :
        m_level(level)/*,
        m_content(content)*/
    {
        (add(args), ...);
    }

    void add(Elem content)
    {
        m_content.push_back(content);
    }


    int Level() const { return m_level; }

    bool operator==(const Heading& rhs) const = default;

    const std::list<Elem>& Items() const { return m_content; }
private:
    int m_level{ 1 };
    //Span m_content; //!< The content of the heading
    std::list<Elem> m_content;
};

class TextDoc
{
public:
    using Elem = std::variant<Heading, Paragraph, Span, std::string>;

    TextDoc() = default;

    template<typename... Args>
    explicit TextDoc(Args&&... args)
    {
        (add(std::move(args)), ...);
    }
    void add(const Elem& item)
    {
        m_items.push_back(item);
    }
    void add(Elem&& item)
    {
        m_items.emplace_back(std::move(item));
    }

    bool operator==(const TextDoc& rhs) const
    {
        return m_items == rhs.m_items;
    }
private:
    std::list<Elem> m_items;
};
}