# Changelog

All notable changes to DocSmith++ are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project aims for
[Semantic Versioning](https://semver.org/). Before 1.0.0, minor versions may contain
breaking changes.

## [Unreleased]

## [0.1.0] - 2026-11-07

First release: a C++20 library for building, reading, editing and writing rich-text
documents through one format-agnostic model. See [DESIGN.md](DESIGN.md) for how
each format maps to it.

### Added
- **Document model**: plain value types in two variants, `inline_node` (`text`,
  `span`, `hyperlink`, `frame`, `bookmark`, `field`, `unknown`) and `block_node`
  (`paragraph`, `heading`, `list`, `table`, `frame`, `page_break`,
  `table_of_contents`, `unknown`). What may contain what is set by the types, and
  content of the wrong level fails to compile at the call. Copy and `==` are
  compiler-generated; fluent setters chain, and move when called on a temporary.
- **Traversal and search**: `walk()`, `collect<T>()`, `find_all<T>()` and
  `strip_styles()` over the body, header and footer; `std::visit` with `overloaded`
  for one level; `io_writer` to print a tree, and debugger visualisers for LLDB and
  Visual Studio in `tools/debug/`.
- **Text**: white space kept as written, `'\t'` for a tab and `'\n'` for a line
  break; spans, links to URLs or bookmarks, bookmarks, page breaks, and fields for
  the page number, page count, date and title.
- **Lists** with list styles (numbered and bulleted levels), continued numbering,
  restarts and unnumbered items.
- **Tables**: always rectangular, with row and column spans, header rows, relative
  column widths, and formatting in layers (table style, column style, header cells,
  cell style, direct formatting), set after the content is in.
- **Images** in frames, with a size and alternative text, held in the document's
  `resources()` so any format can write them; alternative text for tables too.
- **Styles**: a type and a registry per family (`paragraph_style`, `text_style`,
  `table_style`, `cell_style`, `row_style`, `frame_style`, `list_style`), named as
  they are shown in every format, with inheritance. Text properties (font, size,
  weight, italic, colour, underline), paragraph properties (spacing, indents,
  alignment, breaks, keep with next / together) and cell and table properties.
- **Default and builtin styles**: `builtin::para`, `heading(n)`, `link`, `table`
  and `list` style nodes that have none of their own, and `title`, `preformatted`,
  `source_text` and others are known to every writer. The builtins have
  format-agnostic defaults (`builtin::default_style()`), which writers use when a
  document doesn't define them and `styles()[...]` starts from.
- **Page header and footer**, and a **table of contents** whose entries writers
  make from the headings.
- **Metadata**: title, author, language, subject, description, keywords, dates and
  custom properties.
- **Templates**: `text_doc::replace()`, `insert_after()` and `insert_before()` put
  content in place of a placeholder paragraph, found by its text, a bookmark or a
  style; `append()` merges another document.
- **ODT** read and write (`odt::file`). Load, edit and save keeps what the model
  doesn't hold: the rest of the package, unchanged style definitions as they were,
  and unmodelled block and inline content as `unknown`.
- **DOCX** read and write (`docx::file`), with no extra dependency. Load, edit and
  save keeps the rest of the package, and direct formatting (read as styles `T1`,
  `P1`, ...) while it is unchanged.
- **HTML** output (`html::file`): styles as CSS classes, lists as `<ol>` / `<ul>`,
  images written next to the page.
- **Markdown** output (`md::file`), GitHub Flavored, with no dependency; Markdown
  input (`md::parse()`, `md::file::load()`) with md4c, behind the
  `DOCSMITHCPP_MARKDOWN` option.
- **Errors**: reading and writing throw `docsmith::error` with an `error_code`.
- **CMake package** `DocSmithCpp` with the target `docsmith::docsmithcpp`, by
  `find_package`, `add_subdirectory` or FetchContent. Dependencies (pugixml, miniz,
  optionally md4c) are the consumer's to provide.
- **Examples** in `examples/`, the headline one a test report filled in from
  `examples/report_template.odt`.

### Known limitations
- **Untrusted documents** aren't safe to read yet: an image path a document names is
  read from disk when it is saved (and by `md::file::load()`), and archive entries
  are unpacked without a size limit. See the README.
- Frames are always in the line of text: a floating image's position and wrap are
  dropped, and it comes back at its anchor.
- Table cells are text: an ODT cell's value and formula are dropped, so a formula
  cell keeps its last result.
- `unknown` keeps only block- and inline-level content. Unrecognised children inside
  tables and lists, and unknown attributes on known elements, are dropped;
  `tests/odt/test_odt_loss.cpp` lists the cases.
- Editing a loaded style rewrites its definition from the model, so properties the
  model doesn't hold (line spacing, tabs, borders, ...) are dropped from that style.
- ODT can't hold a table style's lines or header in the style, so the writer puts
  them on the cells, and a template saved as `.odt` loses them from the style.
- HTML from a loaded document is plainer than the original, as its CSS covers only
  the properties the model holds. Font sizes in units other than points (e.g.
  `130%`) aren't read.
- One page style per document: no landscape sections or different first pages.
- `set_style()` keeps only the name: the definition goes in `text_doc::styles()`,
  and nothing checks that it is there.

[Unreleased]: https://github.com/MichaelCoutlakis/docsmithcpp/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/MichaelCoutlakis/docsmithcpp/releases/tag/v0.1.0
