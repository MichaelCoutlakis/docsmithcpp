# Changelog

All notable changes to DocSmith++ are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project aims for
[Semantic Versioning](https://semver.org/). Before 1.0.0, minor versions may contain
breaking changes.

## [Unreleased]

The document model was rewritten from a polymorphic class hierarchy into value types
held in `std::variant`. This is a breaking change to almost every public API.

### Changed
- **`table`** keeps its cells private, row by row, and is always rectangular. Read
  them with `cell()`, `row()`, `col()` and `cells()`; add rows with
  `add_row(cells, row_style)`. `header_rows` is now `header_rows()` /
  `set_header_rows()`. `add_row()` with no cells, `add_cell()` and
  `is_rectangular()` are gone. A table also has a style, a name, a style per row,
  and a width and style per column.
- **ODT tables** keep their name, style, row styles and column widths through
  load and save, and the columns and repeated cells and rows LibreOffice writes
  are read.
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
  pugixml and the zip library are no longer exposed to users.
- **miniz replaces libzippp and libzip** (and with them zlib and the compression and
  crypto backends vcpkg pulled in). Archives are read and written in memory by a
  private, format-neutral layer in `src/zip/`, which DOCX can share.
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
- **Paragraph spacing and alignment**: `paragraph_props` holds space above and below
  (`space_before`, `space_after`), indents (`indent_start`, `indent_end`,
  `first_line_indent`, negative for hanging), alignment (`text_align`) and
  `keep_with_next` / `keep_together`. ODT (`fo:margin-*`, `fo:text-indent`,
  `fo:text-align`, `fo:keep-*`) and DOCX (`w:spacing`, `w:ind`, `w:jc`, `w:keepNext`,
  `w:keepLines`) read and write them, and HTML writes them as CSS. So LibreOffice's
  heading spacing now shows in HTML from a loaded `.odt`, and a style defined in code
  can set its own. DOCX writes start and end alignment as left and right.
- **Link styles**: `hyperlink` is styled (a text style), read and written as ODT's
  `text:style-name` on `text:a` (a loaded LibreOffice link keeps "Internet link"), as
  the DOCX run style of the link's text in place of `Hyperlink`, and as an HTML class.
- **Alternative text** for images and tables: `accessibility_title` and
  `accessibility_description` on `frame` and `table`. ODT (`svg:title`, `svg:desc`,
  `table:title`, `table:desc`) and DOCX (`wp:docPr` `title` / `descr`, `w:tblCaption`,
  `w:tblDescription`) read and write them; HTML writes `alt` and `aria-label`, and
  Markdown reads and writes an image's alt text. HTML wrote `alt=""` before.
- **List numbering**: `list::continue_numbering` carries a list's count on from
  the last list of its style, `list_item::start_value` restarts the count at an
  item, and `list_item::numbered` (false) is an item with no number or bullet,
  ODF's list header. ODT reads and writes them (`text:continue-numbering`,
  `text:continue-list`, `text:start-value`, `text:list-header`), DOCX through
  `w:num` instances and `w:startOverride`, and HTML and Markdown write the
  numbers they make. A loaded LibreOffice list that continues after a paragraph
  no longer starts again at 1.
- **Image size**: `frame::width` and `height`, read and written by ODT
  (`svg:width`, `svg:height`) and DOCX (`wp:extent`, also replacing a loaded
  drawing's size), and written by HTML. Before, a loaded `.odt` image was saved
  with no size.
- **Table formatting** in every format (see DESIGN.md, *Tables*). A table style
  (`table_props`) gives the table's width and alignment, its outer and inside
  lines, cell padding and how its header rows' cells look (`header_cells`). How a
  cell looks is `cell_props` (borders, background, vertical alignment, padding and
  a paragraph style for its unstyled paragraphs), in a cell style, a column's
  style (`set_column_style`, applied to rows added later too, read from and written
  to ODT's `table:default-cell-style-name`), the header's, or a cell's direct
  formatting (`table_cell::props`), the later winning.
  `set_borders`, `set_outer_border`, `set_horiz_borders`, `set_vert_borders`,
  `set_row_borders` and `set_column_borders` set lines after the content is in;
  `set_column_widths` sets relative column widths. `effective_cell_props()` says
  how a cell will look. ODT, DOCX and HTML write all of it; ODT and DOCX read it
  back.
- `length` (`pt()`, `mm()`, `cm()`, `inch()`) and `percent`, replacing the unused
  `dim`.
- **DOCX output**: `docx::file(doc).save()` writes a `.docx` with no new
  dependency, on the same private zip layer as ODT. Paragraphs and headings
  (`Heading1`-`9` with `w:outlineLvl`), spans as runs with `w:rStyle`, tabs and
  line breaks, links to URLs and bookmarks, bookmarks, lists through
  `numbering.xml` (each list restarts its numbering), tables with spans and
  repeated header rows, images as inline drawings sized from the PNG, JPEG or GIF
  header, page breaks, `styles.xml` from `styles()` with the builtin code styles
  in Courier New, the page header and footer, `PAGE` / `NUMPAGES` / `DATE` /
  `TITLE` fields, a table of contents field with its entries (and
  `w:updateFields`, so Word offers to add the page numbers), and `meta` in
  `docProps/core.xml` and `custom.xml`. See DESIGN.md, *DOCX*.
- **DOCX input**: `docx::file::load()` reads the same back, from files Word or
  LibreOffice wrote, whatever namespace prefixes they use. Load, edit and save
  keeps the parts the model doesn't represent (theme, footnotes, settings, other
  headers, section settings), elements it doesn't model (as `unknown` nodes), and
  formatting set directly on runs and paragraphs, which is read as styles `T1`,
  `P1`, ... (like ODF's automatic styles) and written back as it was while
  unchanged. A loaded image keeps its size and placement.
- `unknown::source` (`markup::odf` or `markup::ooxml`) says whose markup the node
  holds; each writer writes only its own. `unknown{xml}` is ODF, as before.
- `example_docx_to_html`, and `example_create_document` also writes `.docx`.
- A `libreoffice_opens_output` test (label `libreoffice`) has LibreOffice open
  every `.odt` and `.docx` the tests write, when `soffice` is found.
- **Template-driven test report**: `examples/report_template.odt` (built by
  `example_make_report_template`, then saved by LibreOffice) has heading, body,
  header, footer and pass / fail styles, a header with the title, a footer with
  page numbers, a table of contents and placeholders. `example_create_document`
  fills it in and writes `.odt` and `.html`; `example_fill_template` shows
  replace and insert on their own.
- **Filling in a template**: `text_doc::replace()`, `insert_after()` and
  `insert_before()` put a document's body in place of, after or before a
  `placeholder`: the first paragraph or heading holding a `bookmark`, with a
  `style_name`, or with a given `text`. They search the body (lists and table
  cells included), then the header and footer, and merge styles and resources as
  `append()` does.
- **Table of contents**: a `table_of_contents` block node (`title`, `levels`).
  The ODT writer writes `text:table-of-content` with entries made from the
  headings, linked to them (LibreOffice doesn't regenerate them on load); HTML
  writes a `<nav>` of nested lists, and gives the headings ids to link to.
- **Headers and footers**: `text_doc::page` (`page_style`) holds the header and
  footer as blocks, with `set_header()` / `set_footer()`. ODT reads and writes
  them on the body's master page in `styles.xml`, creating one for a new document
  that has them; HTML writes `<header>` and `<footer>`. `walk()`, `collect()` and
  `find_all()` visit them after the body.
- **Fields**: an inline `field` node for the page number, page count, date and
  document title. ODT writes and reads `text:page-number`, `text:page-count`,
  `text:date` and `text:title`; HTML writes the title and date, and leaves page
  numbers out.
- **Markdown output**: `md::file(doc).to_string()` / `save()` write GitHub Flavored
  Markdown, with no dependency. Bold and italic styles become `**strong**` and
  `*emphasis*`, `builtin::source_text` code spans, `builtin::preformatted` fenced
  code blocks and `Quotations` block quotes. Lists are ordered or not from their list
  style, tables are GFM tables (an empty header row without `header_rows`), bookmarks
  are `<a id>` anchors, `'\n'` is a hard break and `page_break` a thematic break.
  A `table_of_contents` is a nested list of links to the headings, with the same ids
  as in HTML; `field` writes the title or date, and the page header and footer are
  left out. Markdown's special characters are escaped. Images are written next to the
  file, as for HTML. See DESIGN.md, *Markdown*.
- **Markdown input** (CMake option `DOCSMITHCPP_MARKDOWN`, off by default and on in
  the `dev` preset): `md::parse(std::string_view)` and `md::file::load()` read
  CommonMark with GFM tables, strikethrough and autolinks, using md4c. md4c is linked
  privately, and the package config looks for it only when the option is on. Parsed
  emphasis, strong, code and quotes get LibreOffice's style names, so the result
  writes back to the same Markdown and looks right as `.odt`; it can be added to a
  document with `text_doc::append()`. Raw HTML is dropped, apart from bookmark
  anchors and `<br>`.
- `example_md`: builds a test report as Markdown, converts `.odt` to `.md`, and `.md`
  to `.odt` and `.html`.
- **Code blocks**: `builtin::preformatted` (LibreOffice's Preformatted Text) styles a
  monospace paragraph for code and logs, with lines separated by `'\n'`, and
  `builtin::source_text` (Source Text) monospace text in a line. ODT defines them as
  LibreOffice does when the document uses them and doesn't define them; HTML writes
  `<pre>` and `<code>`, also for styles based on them. The example test report has a
  log excerpt.
- **White space, tabs and line breaks** are kept in `text`: spaces as they are,
  `'\t'` for a tab and `'\n'` for a line break, so `paragraph("line 1\nline 2")`
  is two lines. ODT writes them as `text:s`, `text:tab` and `text:line-break` where
  ODF would otherwise collapse them, and reads them back into the text; they used to
  become `unknown`. HTML writes `<br>`, `&nbsp;` and a `white-space: pre` tab.
- **`page_break`** block node: `doc.add(page_break{})` starts a new page. ODT folds it
  into the next paragraph or heading through an automatic style with
  `fo:break-before="page"` (or an empty paragraph if none follows), and reads it back.
  HTML writes `<div style="break-after: page"></div>`.
- **Table header rows**: `table::header_rows` (and `set_header_rows(n)`) counts the
  rows at the top that are header rows. ODT reads and writes them as
  `table:table-header-rows`, so they repeat on each page; the parser used to flatten
  them into ordinary rows. HTML writes them in `<thead>` with `<th>` cells, and the
  other rows in `<tbody>`. The example test report's summary table has one.
- `DESIGN.md` lists the nodes and what each may contain.
- **Loading style definitions**: `odt::file::load()` reads the styles and list styles
  of `styles.xml` and `content.xml` (common and automatic) into `text_doc::styles()` /
  `list_styles()`, as far as the model holds their properties. HTML from a parsed
  `.odt` now gets numbered lists and CSS for its classes.
- The HTML CSS rule for a style includes the properties it inherits from its parent
  styles.
- **Style families**: `style::m_family` (`style_family`: paragraph, text, graphic,
  table, table column, row and cell), set like the other parts:
  `style("Code", style_family::text, ...)`. When it's empty, the ODT writer works it
  out from the nodes that use the style (a `span` → text, a `paragraph` → paragraph,
  ...), and writes a style used on both spans and paragraphs once per family. Styled
  spans now work in ODT; the writer used to write every style as a paragraph style.
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
- A `consumer` ctest that installs the library into the build directory and builds
  `tests/consumer/` against it with `find_package(DocSmithCpp)`, with the same
  toolchain. The README shows the `find_package`, `add_subdirectory` and
  FetchContent routes.
- Debugger visualisers in `tools/debug/`: `docsmith_lldb.py` (CodeLLDB, lldb-dap, lldb)
  and `docsmith.natvis` (Visual Studio, VS Code `cppvsdbg`). They show each node's
  summary, fields and children without the variant, `has_children` and `box`
  plumbing. Setup is in CONTRIBUTING.md.

### Removed
- `graphics_props`, `align_horiz` and `align_vert`, and `style::set(graphics_props)`.
  Frames are always in the line of text, so a graphic style's position had nothing to
  apply to.
- `element.h` (`element`, `element_base`, `elem_tagged`, `elem_t`, `element_visitor`),
  `valid_relations.h`, `parser.h`, `text.h`, `forward_decl.h`, `src/nodes.cpp`,
  `src/list.cpp`.
- `kids` / `kids_tag`, `styled_base`, and the short-lived free builder functions.
- The `DOCSMITHCPP_BUILD_MINIMAL_USAGE` option, which replaced the library with a demo
  executable and was ON by default. `src/odt/basic_main.cpp` became an example.
- The empty placeholder tests in `test_main.cpp`, and the `architecture/` sketches.

### Fixed
- A style whose name spells characters as `_xx_` (`Text_20_body`) gets a
  `style:display-name` ("Text body"). Without it, LibreOffice took a code-built
  `Heading_20_1` for a new style rather than its own Heading 1.
- A font set on a style from code (`font_name`) is declared in `office:font-face-decls`,
  which `style:font-name` refers to. It wasn't declared before.
- `content.xml` and `styles.xml` are written without indentation. Indentation between
  two inline elements (e.g. two spans) reads as a space in LibreOffice. The
  `DOCSMITHCPP_DEBUG_FILES` copies are still indented.
- The ODT parser dropped a space between two inline elements
  (`<text:span>a</text:span> <text:span>b</text:span>`), and kept white space that
  ODF collapses (a run of spaces, a newline in the XML).
- The ODT writer now owns `styles.xml`. Common styles go there, as ODF requires,
  instead of into `content.xml`; a loaded `styles.xml` is updated in place, so its
  page layouts and master pages survive. Saving a loaded file writes each style
  once: a definition that is unchanged since loading keeps its original XML, one
  that was edited replaces it, and a new one is added.
- Common styles in a file saved by an earlier version (in `content.xml`) were lost
  when it was saved again.
- List levels' `text:style-name` was written on the list style instead of the level,
  and `loext:num-list-format` ignored the number's prefix and suffix.
- The parser now reads heading outline levels and style names.
- `odt/file.h` was missing `#pragma once`.
- The installed `DocSmithCppConfig.cmake` now finds its dependencies (pugixml,
  miniz), so `find_package(DocSmithCpp)` works from another project.
- `.odt` files with non-ASCII names can be loaded and saved on Windows. Files are
  opened through `std::filesystem::path`, not `path::string()` (the ANSI code page).
- The ODT `mimetype` entry has its CRC and sizes in its local header, with no data
  descriptor or extra field, as ODF requires of the first entry.
- `gtest_discover_tests` is used with `include(GoogleTest)`. The duplicate catch-all
  `add_test` is gone.
- A parsed document with images couldn't be saved again: the image path pointed
  inside the original archive and the writer looked for it on disk.
- Table cell style names were parsed but never written.
- The writer used the `loext:` namespace without declaring it.
- Frames are anchored as characters. The ODT writer writes `text:anchor-type="as-char"`
  and puts a block frame in an unstyled paragraph of its own, which reads back as a
  block frame; it wrote neither before. A loaded frame's anchor and position (ODT
  `text:anchor-type`, `svg:x`, `svg:y`; DOCX `wp:anchor`) are dropped, so a floating
  picture comes back in the line.
- A loaded `.odt` style's font is its font family, read from the font declaration
  `style:font-name` refers to. It was the declaration's name, which HTML wrote as the
  CSS font family: LibreOffice names a second declaration of a font "Liberation
  Sans1".
- DOCX lists read back as written in three cases: a nested styled list continuing an
  earlier one (`continue_numbering`), a nested list starting with an unnumbered item
  (it read as more of the parent item), and two lists of one style next to each
  other (they read as one list restarting at 1).
- ODT soft page breaks (`text:soft-page-break`) between blocks or in a paragraph, and
  list labels as last laid out (`text:number`), are dropped when read. They were kept
  as `unknown` and written back, so they went stale once the document was edited.

### Known limitations
- HTML from a *parsed* `.odt` is plainer than the original: the CSS covers only the
  properties the style model holds (font, weight, size, colour, paragraph spacing,
  indents, alignment and breaks), not line spacing, tabs or paragraph borders.
- HTML leaves out `unknown` nodes, which hold another format's markup. List number
  prefixes and suffixes aren't reflected in the CSS.
- Font sizes in units other than points (e.g. `130%`) aren't read into the model.
- ODT can't hold a table style's lines or header in the style, so the writer puts
  them on the cells. Reading a file back finds them there, not in the table style,
  so a template saved as `.odt` loses them; define the table style in code.
- Editing a loaded style rewrites its definition from the model, so the properties
  the model doesn't hold (margins, display name, ...) are dropped from that style.
- `unknown` round-trips only block- and inline-level content. Unrecognized children
  inside tables and lists, and unknown attributes on known elements, are dropped;
  `tests/odt/test_odt_loss.cpp` lists the cases (cell values, frame anchoring,
  alt text, ...).
- ODT lengths are read to the thousandth of a point, as they are written.
- Table cells are text: an ODT cell's value and formula (`office:value-type`,
  `office:value`, `table:formula`) are dropped, so a formula cell keeps only its
  last result.
- Images that are no longer referenced stay in the package when a loaded file is
  saved.
- `set_style(style)` keeps only the name. The definition must also be added to
  `text_doc::styles()`.

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
