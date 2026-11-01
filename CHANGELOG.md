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
- **ODT API**: `docsmith::odt::file` replaces `docsmith::odt_file`. Use
  `odt::file::load(path)` and `file.save(path)`; the document is the public `doc`
  member. The file keeps the whole package in memory (`styles.xml`, `meta.xml`,
  settings, thumbnail, embedded images). Load → edit → save writes those parts back
  unchanged, along with the original fonts and automatic styles from `content.xml`.
  `content.xml` itself is regenerated from the model.
- **`text_doc::operator==` compares everything**, the style, list style and resource
  registries included. Compare `a.body == b.body` for content only.
- **Dependencies are private**: `odt/writer.h` moved out of the public headers, so
  pugixml and libzippp are no longer exposed to users.
- **fmt is no longer a dependency.** The writers format their few numbers with
  `std::to_chars` and string concatenation.
- `io_writer` streams its output instead of using `std::format`, so the public
  headers don't need `<format>` (GCC 13, libc++ 17).
- ODT entries other than `mimetype` are now deflated. Previously everything was
  stored uncompressed.
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
- `hyperlink::target` is a `link_target` variant: `url_target` (used as is) or
  `bookmark_target` (a bookmark in the same document). `hyperlink(bookmark{"x"}, ...)`
  links to a bookmark; writers add the `#`, and the ODT parser turns `#x` back into a
  bookmark target. This replaces `hyperlink::url`.
- `text_doc::add_style()` registers a style (or list style) and returns its name in one
  step; `text_doc::append()` appends another document's body, styles and resources and
  throws on a name defined differently in both.
- Text properties `font_weight` and `colour` (`fo:font-weight` / `fo:color` in ODT,
  `font-weight` / `color` in CSS).
- `table::add_row(cells)` checks the row width; `table::is_rectangular()`.
- `unknown` pass-through node. The parser stores verbatim XML for elements it does not
  model and the writer re-emits it.
- `box<T>`: a value-semantic heap holder with type-erased destroy / clone / compare. It
  breaks the recursion between the node variants, so the model compiles on libc++ as
  well as the MSVC STL and libstdc++. It holds a typed `T *`, so debuggers can follow
  it to the held value.
- `styled<Derived>` and `has_children<Child, Derived, Boxed>`: thin non-polymorphic
  CRTP mixins for the style name and the child sequence. `children()` hides the boxing.
- The ODT writer emits `text:outline-level` on headings and `xlink:type` on hyperlinks.
  Empty style names are no longer written.
- Tests: `ValueSemantics`, `ChainedAdd`, `UnknownPassThrough`, `ResaveLoadedFile`,
  `ResaveLoadedImage`, `EditLoadedFile`, `MissingImageThrows`, and an HTML suite
  (escaping, no self-closing tags, CSS, lists, tables, bookmarks, image paths, ODT
  conversion).
- **HTML output**: `docsmith::html::file` writes an HTML5 page (`save(path)` or
  `to_string()`). Style names become CSS classes, and definitions in `doc.styles()`
  become embedded rules (font size, style and family; page and column breaks). Extra
  CSS goes in `css`. A list becomes `<ol>` (with `type` and `start`) or `<ul>`,
  according to its list style at each nesting level. Covered table cells are left out
  and spans become `rowspan` / `colspan`. Bookmarks become ids that `#` links
  resolve to.
- `docsmith.h`: a convenience header for the whole public API.
- `DOCSMITHCPP_DEBUG_FILES` CMake option (ON in the `dev` preset). When set, saving an
  `.odt` also writes `<name>.content.xml` next to it.
- Examples `create_document` (build a document, save it as `.odt` and `.html`, plus
  a test report with a title and metadata), `read_odt` (parse, print the tree, list
  the links) and `odt_to_html`.
- **Document resources**: `text_doc::resources()` holds the bytes and media type of
  each image, by name (`resource`, `resource_registry`; the bytes are shared and
  read-only, so copying a document stays cheap). `add_image(name, data, media_type)`
  adds one and returns the `image`; `write_resources(dir)` writes the ones the body
  refers to. An image `uri` is looked up in the resources, then on disk, then left as
  is. `odt::file::load()` moves referenced images from the package into the
  resources, and `save()` embeds them from there.
- `html::file::resource_dir`: `save()` writes the page's images there (relative to
  the page; empty means next to it). Images from an `.odt` land at
  `resource_dir/Pictures/…`; image files from disk are copied to
  `resource_dir/Pictures/` unless already inside `resource_dir`. `to_string()` writes
  the same `src` values without writing files. Converting a parsed `.odt` to HTML now
  shows its images.
- **Document metadata**: `text_doc::meta` (`metadata`, in `metadata.h`) holds the
  title, author, language, subject, description, keywords, created and modified
  dates, and custom properties. `odt::file` reads it from `meta.xml` and writes it
  back in place, keeping the generator, statistics and unchanged values as they
  were. HTML writes `<html lang>`, `<title>` and `author` / `description` /
  `keywords` meta tags. `html::file::title` now defaults to `doc.meta.title`, then
  the first heading.
- **Title paragraph**: `builtin::title` names the Title paragraph style, which writers
  know without a definition in `text_doc::styles()`. `text_doc::find_title()` returns
  the first paragraph with it, and `set_title(text)` sets that paragraph's text or
  adds one at the start of the body. The ODT writer defines the style as LibreOffice's
  Title (centred, 28pt bold) unless the document or the loaded file already does.
  HTML gives it `class="Title"` and no CSS, and uses its text for `<title>` when
  `meta.title` is empty.
- `named_registry::find` has a `const` overload, and registries compare with `==`.
- `docsmith::docsmithcpp` alias target, so in-tree and installed builds use the same
  target name. The package now installs a version file.
- Debugger visualisers in `tools/debug/`: `docsmith_lldb.py` (CodeLLDB, lldb-dap, lldb)
  and `docsmith.natvis` (Visual Studio, VS Code `cppvsdbg`). They show each node's
  summary, fields and children without the variant, `has_children` and `box`
  plumbing. Setup is in CONTRIBUTING.md.

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
- A parsed document with images couldn't be saved again: the image path pointed
  inside the original archive and the writer looked for it on disk.
- Table cell style names were parsed but never written.
- The writer used the `loext:` namespace without declaring it.

### Known limitations
- HTML from a *parsed* `.odt` is plainer than the original. Style definitions aren't
  loaded (see below), so its classes have no CSS and numbered lists come out as
  bullets. Images embedded in the `.odt` aren't extracted, so they don't show.
- HTML leaves out `unknown` nodes, which hold another format's markup. List number
  prefixes and suffixes, and parent styles, aren't reflected in the CSS.
- `unknown` round-trips only block- and inline-level content. Unrecognized children
  inside tables and lists, and unknown attributes on known elements, are dropped.
- Parsing does not load style definitions into the model, so a parsed document's style
  registries are empty. Style *names* on nodes are kept, and saving a loaded file
  keeps its original definitions, so formatting survives.
- Images that are no longer referenced stay in the package when a loaded file is
  saved.
- Common styles from `text_doc::styles()` are written into `content.xml`. ODF puts
  them in `styles.xml`; LibreOffice accepts this.
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
