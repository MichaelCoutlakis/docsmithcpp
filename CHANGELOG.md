# Changelog

All notable changes to DocSmith++ are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project aims for
[Semantic Versioning](https://semver.org/). Before 1.0.0, minor versions may contain
breaking changes.

## [Unreleased]

The document model was rewritten from a polymorphic class hierarchy into value types
held in `std::variant`. This is a breaking change to almost every public API.

### Changed
- **Document model**: nodes are plain value types grouped into two variants,
  `inline_node` (`text`, `span`, `hyperlink`, `frame`, `bookmark`, `unknown`) and
  `block_node` (`paragraph`, `heading`, `list`, `table`, `frame`, `unknown`). What may
  contain what is set by each child sequence's type. Copy and `operator==` are
  compiler-generated (`= default`), so deep copy and structural equality need no
  `clone()` / `is_equal()`.
- **Construction**: each node has constructors for its own scalar attributes
  (`heading(1)`, `hyperlink("url")`, `table_cell().set_span(2, 2)`), variadic content
  constructors (`paragraph("a", hyperlink(...))`) and a chainable `.add()`. A single
  argument of the node's own type always copies; nesting a same-type node (a span in
  a span) uses `.add()`. This removes the copy-vs-child ambiguity.
- **Traversal and search**: `std::visit` plus the helpers in `visit.h`: `walk`,
  `collect<T>`, `find_all<T>`, `strip_styles` and the `overloaded` idiom. These replace
  `element_base::find_all` / `get_elem_of` and the `element_visitor` base class.
- **Writers**: `io_writer` and the ODT `writer` are recursive `std::visit` emitters with
  no visitor base class.
- **ODT parser** rewritten as recursive descent returning variants, replacing the
  `unique_ptr<element>` factory and stack.
- **Headers** split by concept: `nodes_fwd.h` (forward declarations and mixins),
  `inline_nodes.h`, `list.h`, `table.h`, `nodes.h` (`paragraph`, `heading`,
  `block_node`) and `text_doc.h` (document root and umbrella include).
- **C++20 is required.** `target_compile_features` is now `cxx_std_20` (it was
  `cxx_std_17`, which never matched the code).
- Build: CMake presets restructured into `dev` / `ci` / `release`, with an msys2
  clang64 user preset using the `x64-mingw-static` vcpkg triplet. The presets now set
  the project's real options. They used to set `BUILD_TESTING` / `BUILD_EXAMPLES`,
  which the project never read, so `dev` built neither tests nor examples.
- `DOCSMITHCPP_BUILD_TESTS` and `DOCSMITHCPP_BUILD_EXAMPLES` default to ON only when
  DocSmith++ is the top-level project.
- The test executable changes into `tests/` at startup, so it can be run from any
  directory. Generated test files go to `tests/odt/out/`.

### Added
- `unknown` pass-through node. The parser stores verbatim XML for elements it does not
  model and the writer re-emits it.
- `box<T>`: a value-semantic heap holder with type-erased destroy / clone / compare. It
  breaks the recursion between the node variants, so the model compiles on libc++ as
  well as the MSVC STL and libstdc++.
- `styled<Derived>` and `has_children<Child, Derived, Boxed>`: thin non-polymorphic
  CRTP mixins for the style name and the child sequence. `children()` hides the boxing.
- The ODT writer emits `text:outline-level` on headings and `xlink:type` on hyperlinks.
  Empty style names are no longer written.
- Tests: `ValueSemantics`, `ChainedAdd`, `UnknownPassThrough`.
- `docsmith.h`: a convenience header for the whole public API.
- `DOCSMITHCPP_DEBUG_FILES` CMake option (ON in the `dev` preset). When set, saving an
  `.odt` also writes `<name>.content.xml` next to it.
- Examples `create_basic_odt` (build a document and save it) and `read_odt` (parse,
  print the tree, list the links).
- `docsmith::docsmithcpp` alias target, so in-tree and installed builds use the same
  target name. The package now installs a version file.

### Removed
- `element.h` (`element`, `element_base`, `elem_tagged`, `elem_t`, `element_visitor`),
  `valid_relations.h`, `parser.h`, `text.h`, `forward_decl.h`, `src/nodes.cpp`,
  `src/list.cpp`.
- `kids` / `kids_tag`, `styled_base`, and the short-lived free builder functions.
- The `DOCSMITHCPP_BUILD_MINIMAL_USAGE` option, which replaced the library with a demo
  executable and was ON by default. `src/odt/basic_main.cpp` became an example.
- The empty placeholder tests in `test_main.cpp`, and the `architecture/` sketches.

### Fixed
- The parser now reads heading outline levels and style names.
- `odt/file.h` was missing `#pragma once`.
- The installed `DocSmithCppConfig.cmake` now finds its dependencies (pugixml,
  libzippp, fmt), so `find_package(DocSmithCpp)` works from another project.
- `gtest_discover_tests` is used with `include(GoogleTest)`. The duplicate catch-all
  `add_test` is gone.

### Known limitations
- No HTML output yet.
- `unknown` round-trips only block- and inline-level content. Unrecognized children
  inside tables and lists, and unknown attributes on known elements, are dropped.
- Parsing does not load style definitions (`styles.xml`, automatic styles), so a
  parsed document's style registries are empty. Style *names* on nodes are kept.
- `set_style(style)` keeps only the name. The definition must also be added to
  `text_doc::styles()`.
- The ODT writer always uses `style:family="paragraph"`.

## Pre-release history (untagged)

Development before the changelog existed, summarised from the commit history.

- **2025-11-16**: ODT output of table row and column spans; tables built concisely
  from rows of cells.
- **2025-11-09**: Started tables.
- **2025-09-14**: Bookmarks; paragraph properties with page breaks.
- **2025-08-16**: Search helpers on `element_base`, type tags, `styled_base`; added the
  `architecture/` sketches.
- **2025-08-09**: List styles; `text_doc.h` split into separate files.
- **2025-07-30 to 2025-08-02**: Constructors limited to required data and children,
  with `kids_tag` to disambiguate; groundwork for styles.
- **2025-07-11 to 2025-07-14**: ODT writing, including embedded PNG images.
- **2025-06-26**: Lists and list items; parsing ODT lists; UTF-8 source encoding.
- **2025-06-22**: Moved to conventional polymorphism with mixins; first ODT file
  parsed.
- **2025-06-21**: `.clang-format` / `.clang-tidy`. A first `std::variant` model was
  dropped because of the recursive element relationships. The current model solves
  this with `box<T>`.
- **2025-06-16**: Initial commit.
