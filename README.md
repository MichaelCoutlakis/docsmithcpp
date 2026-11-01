# DocSmith++ (⚠️ 🚧 Note: This is a work in progress 🚧)

**DocSmith++** is a modern C++20 library for reading, modifying, and generating
structured document formats like `.odt`, `.docx`, `.html` and Markdown. It aims
to provide a clean, idiomatic C++ API for working with rich-text documents, with
future support for PDF.

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
| :white_check_mark: | Basic .docx read support              |
| :white_check_mark: | Basic .odt read support               |
| :white_check_mark: | Manipulate the abstract document tree |
| :white_check_mark: | Basic .odt output                     |
| :white_check_mark: | Basic .docx output                    |
| :white_check_mark: | HTML output                           |
| :white_check_mark: | Markdown output                       |
| :white_check_mark: | Markdown input (optional, md4c)       |

## Features

- 📝 Read and write `.odt` (OpenDocument Text) files. Load, edit and save keeps
  the parts of the file the model doesn't represent (styles, metadata, images).
- 📃 Read and write `.docx` (Word) files, with no extra dependency: styles,
  lists, tables, images, headers and footers, fields and a table of contents.
  Load, edit and save keeps the parts and the direct formatting the model
  doesn't represent.
- 🌐 Write HTML5 pages: styles become CSS classes, list styles pick
  `<ol>`/`<ul>`, and images are written next to the page
- 📄 Write GitHub Flavored Markdown (CI job summaries, PR comments): bold and
  italic styles become emphasis, code styles code spans and fenced blocks, and
  tables GFM tables. Read Markdown into the model with md4c (optional)
- 🔍 Query support: search paragraphs, headings, styles, etc.
- ⚙️ Built with modern C++ idioms (the document tree is plain value types in
  `std::variant`, with `std::visit`-based traversal)
- 🔌 Format-agnostic document model: more formats (e.g. PDF) planned

---

## 🔧 Build Instructions

This project uses [CMake](https://cmake.org/) for building and configuring.

### Prerequisites

- C++20 compiler (MSVC ≥ 2019 16.10, GCC ≥ 11, Clang ≥ 14). Builds against
  libstdc++, libc++ and the MSVC STL.
- CMake ≥ 3.20
- Dependencies (e.g. via [vcpkg](https://vcpkg.io/)):
  - [pugixml](https://pugixml.org/)
  - [miniz](https://github.com/richgel999/miniz)
  - [md4c](https://github.com/mity/md4c) (only with `DOCSMITHCPP_MARKDOWN`)
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
| `DOCSMITHCPP_MARKDOWN`       | OFF (ON in `dev`) | Markdown input (`md::parse`, `md::file::load`), with md4c |

### Using it from another CMake project

Whichever route you take, link `docsmith::docsmithcpp`, the same target name in
every case. pugixml and miniz are linked privately, but a static library still
needs them at link time, so they must be findable (e.g. through the same vcpkg
toolchain). So must md4c when the library is built with `DOCSMITHCPP_MARKDOWN`:
the package config then looks for it, and the `docsmith::docsmithcpp` target
defines `DOCSMITHCPP_MARKDOWN` for its users, which `md/file.h` uses to declare
the input functions. Markdown output needs nothing, so it is always built.

Installed, with `find_package`:

```bash
cmake --install build --prefix <prefix>
```

```cmake
find_package(DocSmithCpp CONFIG REQUIRED) # with <prefix> in CMAKE_PREFIX_PATH
target_link_libraries(my_app PRIVATE docsmith::docsmithcpp)
```

From a copy of the source tree, with `add_subdirectory`:

```cmake
add_subdirectory(third_party/docsmithcpp)
target_link_libraries(my_app PRIVATE docsmith::docsmithcpp)
```

With FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(docsmithcpp
    GIT_REPOSITORY https://github.com/MichaelCoutlakis/docsmithcpp.git
    GIT_TAG        main) # or a release tag
FetchContent_MakeAvailable(docsmithcpp)
target_link_libraries(my_app PRIVATE docsmith::docsmithcpp)
```

Tests and examples are only built when DocSmith++ is the top-level project, so
the last two don't build them.

### 🧪 Testing

The tests are unit and integration tests for parsing, writing and transforming
documents. They use small `.odt` fixtures in `tests/odt/` and `.docx` ones,
made by LibreOffice, in `tests/docx/`. Files the tests generate go to
`tests/odt/out/` and `tests/docx/out/`.

When LibreOffice is installed (`soffice` on the `PATH` or in its usual place),
the `libreoffice_opens_output` test runs the tests and has LibreOffice open every
file they wrote. Skip it with `ctest -LE libreoffice`.

The `consumer` test installs the library into the build directory and builds
`tests/consumer/` against it with `find_package`, using the same toolchain. It
configures a project from scratch, so skip it with `ctest -LE consumer` when
iterating.

### Examples

- `example_create_document [name]`: build a document in code and save it as
  `<name>.odt`, `<name>.docx` and `<name>.html`. It also fills in the test report
  template, `examples/report_template.odt`, and saves the report as `.odt`,
  `.docx` and `.html`.
- `example_fill_template`: replace and insert on the report template.
- `example_make_report_template [out.odt]`: build the report template (styles,
  header and footer with page numbers, table of contents, placeholders).
- `example_read_odt <in.odt>`: parse a file, print its tree and list its links.
- `example_odt_to_html <in.odt> [out.html]`: convert through the document model.
- `example_docx_to_html <in.docx> [out.html]`: the same for a Word file.
- `example_md [in.odt|in.md] [out]`: with no arguments, build a small test report
  and save `report.md`; convert an `.odt` to `.md`, or (with
  `DOCSMITHCPP_MARKDOWN`) an `.md` to `.odt` and `.html`.

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
    docx::file(hello).save("hello_world.docx");
    html::file(hello).save("hello_world.html");
    md::file(hello).save("hello_world.md");

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
#ifdef DOCSMITHCPP_MARKDOWN
    doc.append(md::parse("Appended from *Markdown*."));
#endif
    f.save("example_edited.odt");

    // Fill in a template: put content in place of its placeholders, found by
    // their text, a bookmark or a style.
    auto report = odt::file::load("report_template.odt");
    report.doc.replace(text{"{{summary}}"}, {heading(1, "Summary"), paragraph("All passed.")});
    report.doc.page.set_footer(paragraph("Page ", field{field_type::page_number}));
    report.save("report.odt");
    docx::file(report.doc).save("report.docx"); // the same report for Word
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
