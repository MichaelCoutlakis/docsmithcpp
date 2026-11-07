# TODO

Outstanding work, roughly in priority order. Each item says why it matters and
where to start. Done work goes in `CHANGELOG.md` and leaves this file.

## 1. Before 0.1.0

The library builds, installs and passes CI on Linux and MSVC. What's left for a
usable first release is that it doesn't lose content and that the headline use
case, a generated test report, looks like a report.
`write_example_test_report()` in `examples/create_document.cpp` is the
yardstick.

### Tag 0.1.0

Once the above is in, move `[Unreleased]` in `CHANGELOG.md` to `[0.1.0]` and
tag it.

## 2. For 0.2.0

Reading documents from someone else safely, then usability points from a
review of the public API: none of those loses content, but each is a way a new
user is likely to get surprised.

### Untrusted documents

The library assumes the documents it reads are trusted (README, *Untrusted
documents*), which suits generating reports and exporting from an app, but not
a service that converts what it's sent:

- **Files on disk.** An image's uri that names no resource is read from disk
  when the document is saved, and `md::file::load()` reads relative image
  paths that exist. In a file someone else wrote, they choose the path, so
  loading and saving can copy any file the process can read into the output
  (`![](../../.ssh/id_rsa)`). Only files under a trusted root should be read:
  e.g. a root the caller gives, with nothing read when there is none, and an
  error for a path that leaves it. This covers every place the library reads a
  file a document names, not only images.
- **Archive sizes.** An ODT or DOCX entry is unpacked to the size its header
  claims, with no limit, so a small crafted file can make the library allocate
  gigabytes. Reading should limit an entry's size and the total, with an error
  past them.

Add a test for each with a hostile fixture, and update the README note when
it's done.

### New styles don't inherit from the default

`paragraph_style("Note", ...)` takes only properties, so `parent` is set after
construction, and a style added in one line with `styles().insert(...)` has
none. ODF and DOCX derive a style with no parent from the format's base
properties, not from `Standard` / `Normal`, so a font set on `builtin::para`
doesn't reach it (DESIGN.md, *Default styles*). Either take the parent in the
constructor (a `based_on(name)` argument among the properties) or make
`builtin::para` the default parent of a new paragraph style. The second
changes what loaded styles with no parent mean, so it needs care in the
readers. Start in `include/docsmithcpp/style.h`.

### Placeholders within a paragraph, and every match

`text_doc::replace()` and `insert_*()` fill a whole block, the first that
matches. A report's usual placeholder is in a line of text ("Date: {{date}}",
"Run {{id}} passed"), which can't be filled, and one used twice (in the header
and the body) needs a loop. Add a fill for text within a paragraph (inline
content in place of the placeholder's text, across spans), and a way to fill
every match (`replace_all()`, or a count). Start in `src/text_doc.cpp`.

### One way to reach a node's children

`text_doc` has `body`, `page_style` `header` and `footer`; `paragraph`, `span`,
`list_item` and `table_cell` have `items()` and range-for; `list` has `items()`
but no `begin()` / `end()`; `table` has `cells()` and `cell(r, c)`. Code that
works over nodes has to know which. At least give `list` iteration, and decide
whether `text_doc` and `page_style` expose their blocks the same way.

`cell_view` (`table::row()`, `col()`) can't be iterated, sets only the style
and background (no `set_props()`, no borders), and `assign()` with more values
than cells throws partway through, having changed some.

### Traversal ergonomics

`walk()` calls its callback with every node type, `image`, `list_item` and
`table_cell` included, so a lambda for one type fails with a long template
error, and callers write `overloaded{..., [](auto &) {}}` each time. Consider a
typed `walk<T>(doc, f)`, and a `find_first<T>()` (there's no early exit). The
pointers `collect()` and `find_all()` return are invalidated by `add()`,
`replace()` and `insert_*()`, which is easy to trip over when finding a place
and then filling it; say so in `visit.h` at least.

### Reading properties back

Setting a property reads well (`.set(font_size(11))`); reading it back shows
the `m_` naming of the property value types (`style.text.m_font_size->m_points`,
`cell_borders{.m_top = ...}`), next to bare names on nodes and styles
(CONTRIBUTING.md, *Conventions*). Consider bare public names on the value
types, or accessors (`font_size()->points()`).

### Saving a document without copying it

`odt::save(doc, p)` and `docx::save(doc, p)` were removed: next to
`file::save()` they were an easy way to drop what a loaded file keeps beyond
the model. (`html::save()` and `md::save()` stay: those files hold nothing
loaded.) `odt::file(doc).save(p)` copies `doc` into the file (resources are
shared, the tree and style sheet aren't). If the copy matters, add a way that
can't be mistaken for saving a loaded file, e.g. a `file` that refers to a
document rather than owning one, or a name that says the package is new
(`save_new()`).

## 3. After 0.1.0

### Tables

- **Table styles in an ODT template.** ODF can't hold a table style's lines in
  the style, so they go on the cells and a template saved as `.odt` loses them
  from the style; defining the table style in code works in every format.
  ODF's `table:table-template` (LibreOffice's table styles) has a cell style
  per region (first and last row and column, body, odd and even rows and
  columns), which a table switches on (`table:template-name`,
  `table:use-first-row-styles`, ...). Mapping the model's lines and
  `header_cells` onto regions is lossy (corners, header rows past the first,
  lines between cells), and what LibreOffice reads and applies from one needs
  checking first.
- **DOCX header rows** are written as each header cell's own formatting, not as
  the table style's `firstRow` conditional formatting (`tblStylePr`), so Word's
  table style pane doesn't show them. Banding isn't read or written either.
- **More position rules**: first or last column, bands every n rows. Each would
  be a `cell_props` in the table style applied by position, like
  `header_cells`, and written per cell (DESIGN.md, _Tables_), so they work in
  every format without its conditional table styles. Reading could spot a
  style shared down a column and make it the column's style.
- **Row properties** (height, keep together) and absolute column widths.

### Page numbers in the table of contents

The ODT writer writes table of contents entries without page numbers, since it
doesn't lay out pages; LibreOffice adds them when the index is updated. Options:
a macro-free way to have LibreOffice update indexes on load (check whether a
setting exists), or a note in the template.

### Several page styles

`text_doc::page` is one page style for the whole document. A different first
page, or a landscape section, needs a registry of page styles and a way to start
one in the body (ODF master pages, DOCX sections). See DESIGN.md, _Pages,
headers and footers_.

### DOCX gaps

Roughly by how much a report or a loaded file would miss it:

- **Sections**: section breaks inside the body stay in the direct formatting of
  the paragraph that ends the section, so they survive load and save but the
  model can't see them. First-page and even-page headers are kept, not read.
  Both wait for _Several page styles_.
- **Lists**: a table or other non-paragraph block in a list item ends the list
  when read back. Numbering overrides other than the start, `%1.%2.` style
  multi-level numbers (read as the last level only) and list styles linked
  through `w:numStyleLink` aren't modelled.
- **Fields**: fields other than page number, page count, date and title stay
  `unknown`, their shown text included, so it isn't found by text searches.
  Tracked changes, content controls other than a table of contents, and comment
  ranges are `unknown` too.
- **Spans**: runs don't nest, so a span in a span is written with the inner
  style only. A style with no family used on both paragraphs and spans is
  written as `X` and a linked `XChar`, which reads back as two styles.
- **Direct formatting** that is only paragraph-mark run properties
  (`w:pPr/w:rPr`) or a run's language is dropped when read.
- **Images**: a frame's style has no DOCX form and is dropped. An image whose
  size can't be read from its header (SVG, BMP) is written one inch square.
- `_Toc` bookmarks are dropped when read, since the writer makes its own for the
  table of contents; a cross-reference to one in `unknown` markup breaks.
- Strict OOXML (`purl.oclc.org` namespaces) isn't read.

### Model gaps

- A node names its style, of its family. `styles().insert()` registers a
  definition and returns its name, but nothing yet checks that every style a
  node names is defined.
- Bookmark names aren't checked for repeats. Copying a paragraph that holds a
  bookmark repeats its name, and a link to it is then ambiguous. Report
  repeats (with the style check above) rather than renaming them, since a
  writer can't tell which one a link meant.
- Package entries that no `image` referred to at load (e.g. pictures used only
  by `unknown` markup) stay in the ODT package and are carried over on save,
  even if nothing uses them any more. Resources whose image was removed are
  dropped.
- Code blocks are paragraphs styled `builtin::preformatted`, so a Markdown
  fence's info string (` ```c++ `) has nowhere to go and is dropped. Code blocks
  are now as common as headings, so a `code_block` block node (text plus
  `language`) would make them first class. ODT would still write it as a
  Preformatted Text paragraph, keeping the language in a derived style or an
  attribute, and HTML as `<pre><code class="language-…">`.
- **Frame placement**. Frames are always in the line of text. A portable subset
  all of ODT, DOCX and HTML can hold: in the line; a block on its own line,
  left, centred or right, text above and below; floating left or right with
  text wrapping round it (ODT anchored to the paragraph with a graphic style's
  `style:wrap`, DOCX `wp:anchor` with `wrapSquare`, HTML `float`). Offsets,
  page anchors and behind-text would still be dropped. The DOCX writer would
  then write `wp:inline` / `wp:anchor` from the model rather than reuse the
  loaded drawing's.
- `frame` holds only an image. ODF frames can also hold text boxes and objects.
- HTML: images can only be written as files. A `data:` URI mode would give a
  single self-contained page.
- HTML: list number prefixes and suffixes aren't reflected in the CSS.
- HTML: a table's accessibility description is written only when it has no
  title (`aria-label`); `aria-describedby` could carry both.
- Font sizes are points only, so a parsed `130%` is dropped. Editing a loaded
  style rewrites it from the model and drops what the model doesn't hold;
  updating only the modelled attributes in place would keep the rest.
- **Cell values and formulas**. Cells are text: ODT's `office:value` and
  `table:formula` are dropped, so a formula cell keeps only its last result.
  LibreOffice stores a formula cell as the formula, its result as a number and
  the result as text, and a cell's value is used by formulas even with no text
  shown, so carrying any of them over can't stay consistent. The clean model is
  a cell holding a number or a formula, with the writers evaluating formulas and
  writing the text, which needs cell references, number formats and
  recalculation. DOCX table formulas (`{ = A1 }` fields) are `unknown` today, so
  they are carried over and can go stale; they should go the same way as ODT's.
- A link's target frame (`office:target-frame-name`, `w:tgtFrame`) is dropped.
  If it's ever wanted, an enum (`same_window`, `new_window`, ...) rather than
  frame names.

## 4. Internal quality

### Share the image resource plan

`src/resource_plan.{h,cpp}` (where images go when a file is saved with its
images next to it) is used by Markdown. `src/html/file.cpp` still has its own
copy of the same code in an anonymous namespace; switch it to the shared one.

### Namespace prefixes in ODT

pugixml matches names by prefix, not namespace. The DOCX reader renames every
prefix to its own by namespace URI first (`xml::normalize`, `src/xml/`); the ODT
reader still assumes LibreOffice's prefixes (`text:`, `office:`, ...). Use the
same helper there.

### Separate HTML serialisation from the document mapping

`src/html/file.cpp` currently mixes two concerns. One is mapping nodes to
elements. The other is HTML serialisation: escaping, void elements, attribute
quoting, indentation, CSS identifier and id sanitising. Keep the second out of
the docsmith mapping code.

Why not pugixml: it's an XML library, and XML serialisation isn't HTML. It
writes empty elements as `<p/>`, which an HTML parser treats as an _open_ tag.
Its `format_no_empty_element_tags` flag would instead give `<img></img>` for
void elements, which isn't valid HTML either. It also knows nothing about
raw-text elements (`<style>`, `<script>`).

Options:

- **Own small utility** (private, e.g. `src/html/markup.{h,cpp}`): a minimal
  writer or builder with `open(tag, attrs)`, `text()`, `close()`, a void-element
  table and escaping. It's roughly 100–150 lines, has no dependency, and the
  rules are small and well specified. The node mapping would then only describe
  structure. Recommended for now.
- **Third-party**: evaluate licence, upkeep and vcpkg availability before
  relying on any of these.
  - [CTML](https://github.com/tinfoilboy/CTML) (MIT, header-only C++) builds and
    renders HTML trees.
  - [lexbor](https://github.com/lexbor/lexbor) (Apache-2.0, C) is a full HTML5
    parser, DOM and serialiser. Heavier, but it would also cover HTML _import_.
  - Gumbo is parse-only, so it doesn't help with writing.
- Revisit the third-party route if HTML _parsing_ becomes a goal. Then one
  library covering both directions would earn its place.

## Testing

- **What a load and save loses.** `tests/odt/test_odt_loss.cpp` has a test for
  each kind of ODF content the model doesn't hold, saying whether it's kept or
  dropped and why (DESIGN.md, _Document model_). A new case gets a test there.
- **LibreOffice validation in CI.** `libreoffice_opens_output` has LibreOffice
  open every file the tests write, when `soffice` is found. Installing
  LibreOffice (`apt-get install libreoffice-writer`) in one Linux CI job would
  run it on every change. Word itself is stricter than LibreOffice about element
  order; nothing checks DOCX output against it.
- **Fixtures:** add small, targeted `.odt` fixtures per feature (lists, figures,
  bookmarks, page breaks, nested tables, header rows) as tests need them.
  `examples/report_template.odt`, as LibreOffice saved it, covers headers,
  footers, fields and a table of contents.

## Performance (deferred)

- `box<T>` costs one heap allocation per span, hyperlink, list item and table
  cell. The block side could be boxed per list or table instead (`list::items`,
  `table::rows`), leaving `list_item` and `table_cell` unboxed. The inline side
  (span, hyperlink) needs one box per instance.
- If allocation ever shows up in profiles, `box` is the natural place to add an
  arena or pool.

## Debugger display

The visualisers in `tools/debug/` (lldb formatter, natvis; setup in
CONTRIBUTING.md) work with lldb and cdb. Not checked with libstdc++ or gdb;
revisit if that becomes a pain point.

## Docs

The examples are the documentation: each in `examples/` is built in CI, so it
can't go stale, and the README lists them. Keep the `///` comments in the
headers for IDE tooltips, and add an example rather than prose when a use case
isn't covered. A Doxygen reference can wait until someone asks for one.

## CI (low priority)

- **msys2 clang64**: the only libc++ build, but it's the `dev` setup, so it's
  built locally all the time.
- **Clang 14 when ubuntu-22.04 is retired**: it runs there so it gets an older
  libstdc++ than its own. Keep that pairing by running the job in an
  `ubuntu:22.04` container on a newer runner.

## Verifying changes

- Build and test (msys2 clang64 via `CMakeUserPresets.json`):
  `cmake --preset dev`, `cmake --build --preset dev-debug`, `ctest --preset dev`
- MSVC: run `vcvars64.bat`, then configure with Ninja and the MSVC vcpkg
  toolchain (`x64-windows` triplet).
- LibreOffice check: `ctest --preset dev -L libreoffice` runs the tests and
  opens what they wrote. By hand (a file that fails to load prints "source file
  could not be loaded"):
  `soffice.com -env:UserInstallation=file:///<scratch>/lo-profile --headless --convert-to txt:Text --outdir <scratch> tests/odt/out/*.odt`
