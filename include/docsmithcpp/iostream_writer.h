#include "docsmithcpp/text_doc.h"
#include <ostream>

namespace docsmith

{
class io_writer
{
public:
    explicit io_writer(std::ostream &os)
        : m_os(os)
    {
    }
    void operator()(const text_doc &d)
    {
        m_os << "Text Doc:\n";
        for (auto &item : d.items())
            std::visit(*this, item);
    }
    void operator()(const Heading &h)
    {
        m_os << "Heading (level " << h.Level() << "): ";
        for (auto &item : h.Items())
            std::visit(*this, item);
    }
    void operator()(const Paragraph &p)
    {
        m_os << "Paragraph: ";
        for (auto &item : p.Items())
            std::visit(*this, item);
    }
    void operator()(const HyperLink &url)
    {
        m_os << "Hyperlink: " << url.GetURL() << "\n";
    }
    void operator()(const Span &s) { m_os << "Span: " << s.GetText() << "\n"; }
    void operator()(const std::string &s) { m_os << s << "\n"; }

    //void operator()(const List &l)
    //{
    //    m_os << "List: \n";
    //    for (auto &item : l.Items())
    //        // std::visit(*this, item);
    //        this->operator()(item);
    //}

    template <typename Unhandled>
    void operator()(const Unhandled &unhandled)
    {
        m_os << "Unhandled: " << typeid(unhandled).name() << "\n";
    }

    void write(const text_doc &d) { this->operator()(d); }

private:
    std::ostream &m_os;
    int m_indent{0};
};
}