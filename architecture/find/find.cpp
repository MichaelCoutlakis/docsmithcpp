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
#include <list>
#include <memory>
#include <string>
#include <vector>

enum class elem_t
{
    h,
    p,
    li,
    t,
    s,
    doc
};

struct elem_base
{
    virtual ~elem_base() = default;
    // Default element has no children
    virtual std::list<elem_base *> children() { return {}; }

    virtual elem_t type() const = 0;
    virtual bool is_type(elem_t query) const = 0;

    template <typename T>
    std::vector<T *> get_elem_of()
    {
        std::vector<T *> r;

        if(auto *me = dynamic_cast<T *>(this))
            r.push_back(me);

        for(auto *c : children())
        {
            auto kids = c->get_elem_of<T>();
            r.insert(r.end(), kids.begin(), kids.end());
        }
        return r;
    }

    template <typename Predicate>
    auto find_all(Predicate pred) -> std::vector<decltype(pred(std::declval<elem_base *>()))>
    {
        using ElemType = decltype(pred(std::declval<elem_base *>()));
        std::vector<ElemType> r;

        if(auto *p = pred(this))
            r.push_back(p);

        for(auto *c : children())
        {
            auto kids = c->find_all(pred);
            r.insert(r.end(), kids.begin(), kids.end());
        }
        return r;
    }

    template <typename T, typename Predicate>
    std::vector<T *> find_all(Predicate pred)
    {
        std::vector<T *> r;

        if(auto *me = dynamic_cast<T *>(this); me && pred(this))
            r.push_back(me);

        for(auto *c : children())
        {
            auto kids = c->find_all<T>(pred);
            r.insert(r.end(), kids.begin(), kids.end());
        }
        return r;
    }
};

template <typename Derived>
struct child_nodes : virtual elem_base
{
    template <typename Child>
    void add(Child &&child)
    {
        using ChildType = std::decay_t<Child>;
        m_children.emplace_back(std::make_unique<ChildType>(std::move(child)));
    }
    std::list<elem_base *> children() override
    {
        std::list<elem_base *> l;
        for(auto &c : m_children)
            l.push_back(c.get());
        return l;
    }

private:
    std::list<std::unique_ptr<elem_base>> m_children;
};

struct styled_base
{
    virtual ~styled_base() = default;
    virtual void set_style(std::string) = 0;
    virtual const std::string &get_style() const = 0;
};

template <typename Derived>
struct styled : public styled_base
{
    Derived &style(std::string name)
    {
        m_style_name = std::move(name);
        return static_cast<Derived &>(*this);
    }

    void set_style(std::string style_name) override { m_style_name = std::move(style_name); }
    const std::string &get_style() const override { return m_style_name; }

private:
    std::string m_style_name;
};

template <typename Derived, elem_t TypeTag>
struct elem_tagged : virtual elem_base
{
    elem_t type() const override { return TypeTag; }
    bool is_type(elem_t query) const override { return query == TypeTag; }
};

template <typename Derived, elem_t TypeTag>
struct styled_with_child_nodes_and_tag : public child_nodes<Derived>,
                                         public styled<Derived>,
                                         public elem_tagged<Derived, TypeTag>
{
};

struct paragraph : styled_with_child_nodes_and_tag<paragraph, elem_t::p>
{
    paragraph(std::string content) :
        m_content(content)
    {
    }

    std::string m_content;
};

struct heading : styled_with_child_nodes_and_tag<heading, elem_t::h>
{
    heading(std::string content) :
        m_content(content)
    {
    }
    std::string m_content;
};

struct text_doc : public child_nodes<text_doc>, elem_tagged<text_doc, elem_t::doc>
{
    elem_t type() const override { return elem_t::doc; }
};

int main()
{
    text_doc d;
    d.add(heading{"The heading"}.style("H1"));
    d.add(paragraph{"The paragraph"}.style("P1"));

    auto h = d.get_elem_of<heading>();
    auto p = d.get_elem_of<paragraph>();

    auto s = d.get_elem_of<styled_base>();

    auto styled_not_heading = d.find_all(
        [](elem_base *p) -> styled_base *
        {
            auto styled_cast = dynamic_cast<styled_base *>(p);
            return styled_cast && !p->is_type(elem_t::h) ? styled_cast : nullptr;
        });

    auto s2 = d.find_all<styled_base>([](elem_base *p) { return !p->is_type(elem_t::h); });

    return 0;
}
