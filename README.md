# DocSmith++ (⚠️ 🚧 Note: This is a work in progress 🚧)

**DocSmith++** is a modern C++20 library for reading, modifying, and generating
structured document formats like `.odt` and `.html`. It aims to provide a clean,
idiomatic C++ API for working with rich-text documents, with future support for
`.docx`, Markdown, and PDF.

## Typical use case scenarios

### Automatic report generation

Test software written in C++ could use `docsmithcpp` to generate test reports.
`docsmithcpp` provides an API to do this cleanly without:

- cluttering the code with HTML/Markdown string processing, which can get messy
  quickly, for example once sections need to be linked or strings escaped.
- needing intermediate formats or further post processing tools which would be
  needed if the report was exported as an intermediate format and then post-
  processed.

### Native document generation from C++

- Document export from C++ application
- Template based documents: Load a template, fill in data, and save

## When not to use DocSmith++

- Document processing from a native C++ application is not required. See the
  [Bibliography](#bibliography) or search engine for document libraries in other
  languages
- Once-off conversions or formatting: `pandoc` is the appropriate tool here
- Typesetting or maths-heavy documents: use `LaTeX`.

---

### 🚧 Roadmap

| Status             | Feature                               |
| ------------------ | ------------------------------------- |
| :x:                | Basic .docx read support              |
| :white_check_mark: | Basic .odt read support               |
| :white_check_mark: | Manipulate the abstract document tree |
| :white_check_mark: | Basic .odt output                     |
| :x:                | Basic .docx output                    |
| :white_check_mark: | HTML output                           |

## Features

- 📝 Read and write `.odt` (OpenDocument Text) files. Load, edit and save keeps
  the parts of the file the model doesn't represent (styles, metadata, images).
- 🌐 Write HTML5 pages: styles become CSS classes, list styles pick
  `<ol>`/`<ul>`, and images are written next to the page
- 🔍 Query support: search paragraphs, headings, styles, etc.
- ⚙️ Built with modern C++ idioms (the document tree is plain value types in
  `std::variant`, with `std::visit`-based traversal)
- 🔌 Format-agnostic document model: more formats (e.g. `.docx`, Markdown, PDF)
  planned

---

## 🔧 Build Instructions

This project uses [CMake](https://cmake.org/) for building and configuring.

### Prerequisites

- C++20 compiler (MSVC ≥ 2019 16.10, GCC ≥ 11, Clang ≥ 14). Builds against
  libstdc++, libc++ and the MSVC STL.
- CMake ≥ 3.20
- Dependencies (e.g. via [vcpkg](https://vcpkg.io/)):
  - [pugixml](https://pugixml.org/)
  - [libzippp](https://github.com/ctabin/libzippp) (and
    [libzip](https://libzip.org/))
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

| Option                       | Default           | Effect                                                    |
| ---------------------------- | ----------------- | --------------------------------------------------------- |
| `DOCSMITHCPP_BUILD_TESTS`    | ON when top-level | Build the GoogleTest suite                                |
| `DOCSMITHCPP_BUILD_EXAMPLES` | ON when top-level | Build `examples/`                                         |
| `DOCSMITHCPP_DEBUG_FILES`    | OFF (ON in `dev`) | Also write `<name>.content.xml` next to each saved `.odt` |

### 🧪 Testing

The tests are unit and integration tests for parsing, writing and transforming
documents. They use small `.odt` fixtures in `tests/odt/`. Files the tests
generate go to `tests/odt/out/`.

### Examples

- `example_create_document [name]`: build a document in code and save it as
  `<name>.odt` and `<name>.html`.
- `example_read_odt <in.odt>`: parse a file, print its tree and list its links.
- `example_odt_to_html <in.odt> [out.html]`: convert through the document model.

📚 Example Usage

```c++
#include <docsmithcpp/docsmith.h>
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
    odt::file(hello).save("hello_world.odt");
    html::file(hello).save("hello_world.html");

    // Load, query and edit. Saving keeps the parts of the package the model
    // doesn't represent (styles.xml, meta.xml, embedded images, ...).
    auto f = odt::file::load("example.odt");
    text_doc &doc = f.doc;

    // Find nodes by type (optionally with a predicate) -- no dynamic_cast.
    for (const hyperlink* a : collect<hyperlink>(doc))
        if (const auto* u = std::get_if<url_target>(&a->target))
            std::cout << u->url << "\n";

    auto body_paras = find_all<paragraph>(doc,
        [](const paragraph& p) { return p.style.get_name() == "Text_body"; });

    // Visit a node directly with the overloaded idiom.
    for (const block_node& b : doc.body)
        std::visit(overloaded{
            [](const heading& h) { std::cout << "heading L" << h.level << "\n"; },
            [](const auto&)      { /* other block types */ },
        }, b.v);

    doc.add(paragraph("Appended from C++."));
    f.save("example_edited.odt");
}
```

## Design notes

See [DESIGN.md](DESIGN.md) for the document model and other design decisions.

## Bibliography

Worthy mentions and related projects

- [odfpy](https://github.com/eea/odfpy/wiki/Introduction) Is a python library
  (GPL) to read and write ODF text documents. It has an API similar to what I'm
  trying to achieve with `docsmithcpp`, namely, in terms of generating and
  manipulating the document structure (e.g.
  `h=H(outlinelevel=1, stylename=h1style, text="My first text")`,
  `doc.getElementsByType`)
- [python-docx](https://python-docx.readthedocs.io/en/latest/), a Python library
  for creating and updating .docx files. The document generation API is similar
  to what I'm trying to achieve.
- [docx4j](https://www.docx4java.org/trac/docx4j) is a Java library for creating
  and manipulating docx adn other OOXML files. However, it looks to be on the
  heavier and more verbose side. E.g.
  `WordprocessingMLPackage wordPackage = WordprocessingMLPackage.createPackage();`
