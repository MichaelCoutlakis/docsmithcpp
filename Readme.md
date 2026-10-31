# DocSmith++ (⚠️ 🚧 Note: This is a work in progress 🚧)

**DocSmith++** is a modern C++20 library for reading, modifying, and generating
structured document formats like `.docx` and `.odt`. It aims to provide a clean,
idiomatic C++ API for working with rich-text documents, with future support for
HTML, Markdown, and PDF.

## Typical use case scenarios

asdf

---

### 🚧 Roadmap

| Status | Feature                               |
| ------ | ------------------------------------- |
| :x:    | Basic .docx read support              |
| :x:    | Basic .odt read support               |
| :x:    | Manipulate the abstract document tree |
| :x:    | Basic .odt output                     |
| :x:    | Basic .docx output                    |
| :x:    | HTML output                           |

## Features

- 📝 Read and write `.odt` (OpenDocument Text) files
- 🔍 Query support: search paragraphs, headings, styles, etc.
- ⚙️ Built with modern C++ idioms (the document tree is plain value types in
  `std::variant`, with `std::visit`-based traversal)
- 🔌 Flexible design: future extensions (e.g., HTML/PDF) planned

---

## 🔧 Build Instructions

This project uses [CMake](https://cmake.org/) for building and configuring.

### Prerequisites

- C++20 compiler (MSVC ≥ 2019 16.10, GCC ≥ 11, Clang ≥ 14). Builds against
  libstdc++, libc++ and the MSVC STL.
- CMake ≥ 3.20
- Dependencies (e.g. via [vcpkg](https://vcpkg.io/)):
  - [pugixml](https://pugixml.org/)
  - [libzippp](https://github.com/ctabin/libzippp) (and [libzip](https://libzip.org/))
  - [fmt](https://fmt.dev/)
  - [gtest](https://github.com/google/googletest) (tests only)

### Build

The repository ships `dev`, `ci` and `release` configure presets. They contain
no toolchain, so put your compiler and vcpkg toolchain file in a
`CMakeUserPresets.json` base preset named `user-base`.

```bash
cmake --preset dev
cmake --build --preset dev-debug
ctest --preset dev
```

Without presets:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
ctest --test-dir build
```

| Option                       | Default            | Effect                                                     |
| ---------------------------- | ------------------ | ---------------------------------------------------------- |
| `DOCSMITHCPP_BUILD_TESTS`    | ON when top-level  | Build the GoogleTest suite                                 |
| `DOCSMITHCPP_BUILD_EXAMPLES` | ON when top-level  | Build `examples/`                                          |
| `DOCSMITHCPP_DEBUG_FILES`    | OFF (ON in `dev`)  | Also write `<name>.content.xml` next to each saved `.odt`  |

### 🧪 Testing

The tests are unit and integration tests for parsing, writing and transforming
documents. They use small `.odt` fixtures in `tests/odt/`. Files the tests
generate go to `tests/odt/out/`.

### Examples

- `example_create_basic_odt [out.odt]`: build a document in code and save it.
- `example_read_odt <in.odt>`: parse a file, print its tree and list its links.

📚 Example Usage

```c++
#include <docsmithcpp/odt/file.h>
#include <docsmithcpp/text_doc.h>
#include <iostream>

using namespace docsmith;

int main() {
    // Build a document from code. Nodes are plain values; bare strings become
    // text. Constructors take a node's scalar attributes; children are passed as
    // extra arguments or appended with .add() -- so there is no copy-vs-child
    // ambiguity (nesting a same-type node, e.g. a span in a span, uses .add()).
    text_doc hello{
        heading(1, "Hello World!"),
        paragraph("This is the standard example with a ",
                  hyperlink("https://example.com", "link"), "."),
        paragraph().set_style("Quote").add("built incrementally"),
    };
    odt_file("hello_world.odt").save(hello);

    // Parse, query, and edit.
    text_doc doc = odt_file("example.odt").parse_text_doc();

    // Find nodes by type (optionally with a predicate) -- no dynamic_cast.
    for (const hyperlink* a : collect<hyperlink>(doc))
        std::cout << a->url << "\n";

    auto body_paras = find_all<paragraph>(doc,
        [](const paragraph& p) { return p.style.get_name() == "Text_body"; });

    // Visit a node directly with the overloaded idiom.
    for (const block_node& b : doc.body)
        std::visit(overloaded{
            [](const heading& h) { std::cout << "heading L" << h.level << "\n"; },
            [](const auto&)      { /* other block types */ },
        }, b.v);
}
```

## Design notes

The document tree is a pair of value-type variants — `inline_node` (text-flow
content: `text`, `span`, `hyperlink`, `frame`, `bookmark`, `unknown`) and
`block_node` (`paragraph`, `heading`, `list`, `table`, `frame`, `unknown`). It
is deliberately a _generic_, format-agnostic model: which group may go where is
encoded by the child sequence's type, so there is no separate per-format
validation layer. Copy and `operator==` are compiler-generated, and traversal is
`std::visit`. Shared behaviour lives in thin, non-polymorphic CRTP mixins:
`styled<Derived>` (a style name + `set_style`) and `has_children<…>`
(`children()`/`add()`).

An `unknown` node stores verbatim XML for anything the parser does not model, so
foreign content survives a parse → write round trip. Note: this currently
applies to block- and inline-level content only — unrecognized children _inside_
a table or list, and unknown attributes on known elements, are not yet captured.

The recursive edges (a `span` contains inline nodes, which contain spans…) are
held through a small value-semantic wrapper, `box<T>`
(`include/docsmithcpp/box.h`), which keeps the model portable across libstdc++,
libc++ and the MSVC STL.

## Bibliography

Worthy mentions and related projects

- [odfpy](https://github.com/eea/odfpy/wiki/Introduction) Is a python library
  (GPL) to read and write ODF text documents. It has an API similar to what I'm
  trying to acheive with docsmithcpp, namely, in terms of generating and
  manipulating the document structure (e.g.
  `h=H(outlinelevel=1, stylename=h1style, text="My first text")`,
  `doc.getElementsByType`)
- [python-docx](https://python-docx.readthedocs.io/en/latest/), a Python library
  for creating and updating .docx files. The document generation API is similar
  to what I'm trying to acheive.
- [docx4j](https://www.docx4java.org/trac/docx4j) is a Java library for creating
  and manipulating docx adn other OOXML files. However, it looks to be on the
  heavier and more verbose side. E.g.
  `WordprocessingMLPackage wordPackage = WordprocessingMLPackage.createPackage();`
