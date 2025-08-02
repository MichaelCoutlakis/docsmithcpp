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
#include <string>

namespace docsmith
{

enum class align_horiz
{
    left,
    centre,
    right,
};

enum class align_vert
{
    top,
    centre,
    bottom,
};

class list;

class style_name
{
public:
    style_name() = default;

    style_name(const char *name) :
        style_name(std::string(name))
    {
    }
    style_name(const std::string &name) :
        m_name{name}
    {
    }

    const std::string &get_name() const { return m_name; }
    bool is_empty() const { return m_name.empty(); }
    bool operator==(const style_name &rhs) const = default;

private:
    std::string m_name;
};

struct font_name
{
public:
    explicit font_name(const std::string &font_name) :
        m_font_name{font_name}
    {
    }

private:
    std::string m_font_name;
};

enum class font_style
{
    normal,
    italic,
    oblique
};
enum class font_weight
{
    normal,
    bold
};

struct font_size
{
    explicit font_size(float points) :
        m_points{points}
    {
    }
    float m_points;
};

template <typename Derived>
class styled
{
public:
    styled() = default;
    explicit styled(style_name sn) :
        m_style_name(std::move(sn))
    {
    }
    Derived &set_style(style_name sn)
    {
        m_style_name = std::move(sn);
        return static_cast<Derived &>(*this);
    }

    template<typename HasStyle>
    Derived &set_style(const styled<HasStyle> &has_style)
    {
        return set_style(style_name(has_style.get_style()));
    }
    const style_name &get_style() const { return m_style_name; }

private:
    style_name m_style_name;
};
};