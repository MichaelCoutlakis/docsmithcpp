# DocSmith++ (⚠️ 🚧 Note: This is a work in progress 🚧)

**DocSmith++** is a modern C++17+ library for reading, modifying, and generating structured document formats like `.docx` and `.odt`. It aims to provide a clean, idiomatic C++ API for working with rich-text documents, with future support for HTML, Markdown, and PDF.

---

### 🚧 Roadmap
| Status | Feature |
| --- | --- |
| :x: | Basic .docx read support |
| :x: | Basic .odt read support |
| :x: | Manipulate the abstract document tree |
| :x: | Basic .odt output |
| :x: | Basic .docx output |


## Features

- 🧾 Read and write `.docx` (Office Open XML) documents
- 📝 Read and write `.odt` (OpenDocument Text) files
- 🔍 Query support: search paragraphs, headings, styles, etc.
- ⚙️ Built with modern C++ idioms (Value semantics, RAII, smart pointers, STL)
- 🔌 Flexible design: future extensions (e.g., HTML/PDF) planned

---

## 🔧 Build Instructions

This project uses [CMake](https://cmake.org/) for building and configuring.

### Prerequisites
- C++17 compiler (GCC ≥ 9, Clang ≥ 10, MSVC ≥ 2019)
- CMake ≥ 3.16
- Dependencies:
  - [libzip](https://libzip.org/)
  - [libzippp](https://github.com/ctabin/libzippp)
  - [pugixml](https://pugixml.org/)
  - [gtest](https://github.com/google/googletest)

### Build Example

```bash
cd DocSmithCpp
mkdir build && cd build
cmake ..
make
```

### 🧪 Testing

DocSmithCpp includes unit and integration tests for document parsing, writing, and transformation. Tests rely on a collection of small .docx and .odt files under the tests directory.

To run tests:

cmake -DBUILD_TESTING=ON ..
make
ctest

📚 Example Usage

```c++
#include <docsmith/odt/odt_file.h>
#include <docsmith/docx/docx_file.h>

using namespace docsmith;

int main() {
    ODT_File file("example.odt");
    TextDoc doc = file.parse_text_doc();

    auto SearchPredicate = [](const Paragraph& p) { return p.style_name() == "name_to_look_for";};

    for (const auto& para : doc.find_all<Paragraph>(SearchPredicate)) 
    {
        std::cout << para.text() << std::endl;
    }
    TextDoc hello_world{Heading(1, "Hello World!"), Paragraph(This is the standard example)};
    DOCX_File docx("hello_world.docx");
    docx.set_content(hello_world);
    docx.save();
}
```


## Bibliography
Worthy mentions and related projects
- [odfpy](https://github.com/eea/odfpy/wiki/Introduction) Is a python library (GPL) to read and write ODF text documents. It has an API similar to what I'm trying to acheive with docsmithcpp, namely, in terms of generating and manipulating the document structure (e.g. `h=H(outlinelevel=1, stylename=h1style, text="My first text")`, `doc.getElementsByType`)
- [python-docx](https://python-docx.readthedocs.io/en/latest/), a Python library for creating and updating .docx files. The document generation API is similar to what I'm trying to acheive.
- [docx4j](https://www.docx4java.org/trac/docx4j) is a Java library for creating and manipulating docx adn other OOXML files. However, it looks to be on the heavier and more verbose side. E.g. `WordprocessingMLPackage wordPackage = WordprocessingMLPackage.createPackage();`