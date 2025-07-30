#include "docsmithcpp/iostream_writer.h"
#include "docsmithcpp/text_doc.h"
#include <iostream>

int main()
{
    using namespace docsmith;
    // text_doc doc;

    // // heading h1(1);
    // //
    // // h1.add(text("This is the heading"));

    //// heading h1 = heading(1).add(text("This is the heading"));
    // auto s1 = span().add(text("some text here"));
    // //auto h1 = heading(1).add(text("This is the first heading")).add(span(text("some
    // span"))).set_style("some_style");

    // auto h1 = heading(1, text("This is the heading text")).set_style("H1 style name");

    // list_item l1(paragraph("hello"));

    // span s2(text("hello"), text("hello"), span(text("asdf")));

    /*span s{span{span{span{}.add(text("hello"))}}};*/
    //span s{kids{span{kids{span{kids{span{}}}}}}};

    //span s{kids{text{"hello"}, span{kids{text{"hello"}}}}};

    //span s{kids{span{kids{text{"Github"}}}}};

    //span s{kids_tag{}, span{kids_tag{}, text{"hello"}}};

    using tg = kids_tag;
    constexpr kids_tag in{};
    span s{in, span{text{"hello"}}};

     //auto s = span{}.add(span{}.add(text{"hello"}));

    io_writer w(std::cout);
    s.accept(w);
    return 0;
}
