# TODO

Outstanding work, roughly in priority order. Each item says why it matters and
where to start. See `CHANGELOG.md` for what's done.

The order follows what stops someone else from using the library. First they
need to be able to get it and build against it. Then it has to be correct. Then
the headline use case (a generated test report) has to look like a report.
More formats come after that.

## 1. Usable by others (blocks 0.1.0)

### CI
GitHub Actions for msys2 clang64, MSVC, and **Linux GCC and Clang, which have
never been built**. The README claims GCC ≥ 11 and libstdc++ support, and nothing
checks either. Include a GCC 11 job so the stated minimum is tested, not just
the newest compiler. Run the `consumer` test (`tests/consumer/`) in at least
one job.

### Tag 0.1.0
Do this once the items above pass. Move `[Unreleased]` in `CHANGELOG.md` to
`[0.1.0]` and tag it.

## 2. Correctness

### Content lost inside tables and lists
`unknown` round-trips only block- and inline-level content. Unrecognised children
*inside* tables and lists, and unknown *attributes* on known elements, are
dropped. So load, edit, save of a real-world file can silently lose content.

## 3. Reports that look like reports

`write_example_test_report()` in `examples/create_document.cpp` is the yardstick.

### The test report example loads a template
The example should load a template `.odt` (e.g. `examples/report_template.odt`)
that already has basic styling: fonts, heading styles, a table style, and a
header and footer. It then fills in the body and saves. This is the "load a
template, fill in data, save" scenario from the README. It needs style
definitions to be parsed (section 2) so that HTML output from the same document
gets the template's look too.

### Filling in a template
There is no fill-in mechanism. Today, filling a template means searching `body`
by hand. Options: replace a bookmark, or a paragraph with a placeholder style or
text, with a sequence of nodes. Insert-after or replace helpers on `text_doc`
would cover most of it.

### Table and cell properties
The style model covers only text, paragraph and graphics properties. It has no
borders, column widths, cell shading or vertical alignment, so a results table
in ODT has no borders at all. Add table, column and cell style properties.

### Table header rows
The parser flattens `table:table-header-rows` into ordinary rows, so header rows
are lost: in ODT they don't repeat on each page, and HTML has no
`<thead>`/`<th>`. Model them.

### Line breaks, tabs and spaces
ODT `text:s` (repeated spaces), `text:tab` and `text:line-break` become
`unknown`, so extracting the plain text loses them, and there's no way to build
them in code. You can't put a multi-line log excerpt in a cell. Model them as
inline nodes.

### Code / preformatted blocks
Test reports need monospace blocks for logs and output. This could be a
paragraph style plus the line-break and space nodes above. In HTML it would be
`<pre>`.

### Table of contents
- ODT: `text:table-of-content`. LibreOffice shows the entries stored in the file
  and doesn't regenerate them on load, so the writer has to write the entries
  itself, built from the headings.
- HTML: a `<nav>` list of links to the headings, which need ids or bookmarks.

### Headers, footers and page numbers
- ODT: headers and footers belong to a master page (`style:master-page` with
  `style:header` / `style:footer`) in `styles.xml`. A template already has them,
  and the writer keeps a loaded `styles.xml`, so they work today without a model.
  Creating them from code needs a page / master-page model; the writer already
  owns `styles.xml` (`writer::load_styles_xml()` in `src/odt/writer.cpp`).
- Page numbers are fields (`text:page-number`, `text:page-count`). They need a
  small field node, which also suits dates and the document title.
- HTML has no pages. Write an optional `<header>` / `<footer>` and leave page
  numbers out, or use print CSS.

## 4. Formats

### Markdown output
Writing Markdown is cheap and fits the test report case: CI job summaries and PR
comments are Markdown. It's also the first format that isn't ODF or HTML, so it
tests whether the model really is format-agnostic. Write it ourselves (GFM
tables, links, bookmarks as anchors). Styles have no Markdown form, so decide
which ones map to emphasis or code.

### Markdown input
Shape the API like the others, e.g. `md::parse(std::string_view) -> text_doc`
and `md::file::load/save/to_string`, rather than `text_doc::from_md`, so the core
stays format-agnostic. A parsed snippet can then be added with
`text_doc::append()`.

Use [md4c](https://github.com/mity/md4c) for parsing rather than our own parser.
It's MIT, two C files with no dependencies, supports CommonMark and the GFM
extensions (tables, strikethrough, task lists), and is in vcpkg. Its SAX-style
callbacks (`enter_block`, `leave_block`, `text`) map directly to building the
tree with a stack. Reasons not to write our own: CommonMark's edge cases
(emphasis delimiter runs, lazy continuation lines, tight and loose lists, link
reference definitions) are where hand-written parsers go wrong, and a subset
parser would quietly define its own dialect.

Link md4c privately, like pugixml, and put Markdown input behind an option,
`DOCSMITHCPP_MARKDOWN`, so consumers who don't need it don't need md4c. Markdown
output has no dependency and stays unconditional.

### DOCX
This is the format most users of a C++ document library will need. Output
first, then input. DOCX is an OPC package, a zip like ODF with
`[Content_Types].xml` in place of the manifest, so it should share the zip layer
(`src/zip/`). Note that pugixml matches namespace *prefixes*, not URIs. That's
fine for ODF as LibreOffice writes it, but DOCX reading should not assume `w:`.

## 5. Dependencies

The goal is a library a consumer can build without a package manager. The
dependencies are pugixml and miniz, both MIT and small.

### Keep pugixml
It's MIT, one `.cpp`, widely packaged, and private. Writing XML ourselves would
be easy, but parsing (entities, namespaces, CDATA) isn't worth owning.

### Fallbacks for pugixml and miniz
Offer a vendored or FetchContent fallback for each alongside `find_package`.
miniz is a single `.c`/`.h` pair, so vendoring it is cheap. Its licence file
goes next to it (CONTRIBUTING, *Adding a dependency*).

## 6. Internal quality

### Separate HTML serialisation from the document mapping
`src/html/file.cpp` currently mixes two concerns. One is mapping nodes to
elements. The other is HTML serialisation: escaping, void elements, attribute
quoting, indentation, CSS identifier and id sanitising. Keep the second out of
the docsmith mapping code.

Why not pugixml: it's an XML library, and XML serialisation isn't HTML. It writes
empty elements as `<p/>`, which an HTML parser treats as an *open* tag. Its
`format_no_empty_element_tags` flag would instead give `<img></img>` for void
elements, which isn't valid HTML either. It also knows nothing about raw-text
elements (`<style>`, `<script>`).

Options:
- **Own small utility** (private, e.g. `src/html/markup.{h,cpp}`): a minimal
  writer or builder with `open(tag, attrs)`, `text()`, `close()`, a void-element
  table and escaping. It's roughly 100–150 lines, has no dependency, and the rules
  are small and well specified. The node mapping would then only describe
  structure. Recommended for now.
- **Third-party**: evaluate licence, upkeep and vcpkg availability before relying
  on any of these.
  - [CTML](https://github.com/tinfoilboy/CTML) (MIT, header-only C++) builds and
    renders HTML trees.
  - [lexbor](https://github.com/lexbor/lexbor) (Apache-2.0, C) is a full HTML5
    parser, DOM and serialiser. Heavier, but it would also cover HTML *import*.
  - Gumbo is parse-only, so it doesn't help with writing.
- Revisit the third-party route if HTML *parsing* becomes a goal. Then one
  library covering both directions would earn its place.

### Model gaps
- `unknown` holds ODT XML but doesn't say which format it came from. Record the
  source format so writers for other formats (Markdown, DOCX) can decide what to
  do with it.
- `set_style(style)` keeps only the name. `text_doc::add_style()` registers a
  definition and returns its name, but nothing yet checks that every style a node
  names is defined.
- Package entries that no `image` referred to at load (e.g. pictures used only by
  `unknown` markup) stay in the ODT package and are carried over on save, even if
  nothing uses them any more. Resources whose image was removed are dropped.
- `frame` holds only an image. ODF frames can also hold text boxes and objects.
- HTML: images can only be written as files. A `data:` URI mode would give a
  single self-contained page.
- HTML: list number prefixes and suffixes aren't reflected in the CSS.
- Font sizes are points only, so a parsed `130%` is dropped. Editing a loaded
  style rewrites it from the model and drops what the model doesn't hold;
  updating only the modelled attributes in place would keep the rest.

## Testing
- **Optional LibreOffice validation test.** Headless LibreOffice found a bug every
  unit test missed (an `office:version` mismatch made it reject resaved files).
  Add a ctest that runs only when `soffice` is found and converts every file in
  `tests/odt/out/` (see *Verifying* below).
- **Fixtures:** the old experimental samples were deleted. Add small, targeted
  `.odt` fixtures per feature (lists, figures, bookmarks, page breaks, nested
  tables, header rows, headers and footers) as tests need them.

## Performance (deferred)
- `box<T>` costs one heap allocation per span, hyperlink, list item and table cell.
  The block side could be boxed per list or table instead
  (`list::items`, `table::rows`), leaving `list_item` and `table_cell` unboxed. The
  inline side (span, hyperlink) needs one box per instance.
- If allocation ever shows up in profiles, `box` is the natural place to add an
  arena or pool.

## Debugger display
`box<T>` holds a typed `T *`, and `tools/debug/` has an lldb formatter and a natvis
file (setup in CONTRIBUTING.md). Checked with lldb 23 and CodeLLDB's lldb 22
(clang64, libc++) and cdb (MSVC). Not yet checked in the VSCodium UI.

- Not checked with libstdc++ (no g++ toolchain here). The lldb formatter reads the
  variant through lldb's own std::variant formatter, so it should work there too.
- The natvis reads MSVC STL internals. With `cppdbg` (gdb or lldb-mi, other
  standard libraries) the node variants fall back to showing `.v`, and the child
  lists don't expand.
- Possibly gdb pretty printers.

## Docs and project
- Write a short spec of the generic document model in `DESIGN.md`: which nodes
  exist and which may contain which, independent of any one format. It would
  guide new nodes and new formats.
- Add a Doxygen target. The `///` comments are there, but nothing generates an
  API reference.
- The README's "Typical use case scenarios" section is a placeholder.

## Verifying changes
- Build and test (msys2 clang64 via `CMakeUserPresets.json`):
  `cmake --preset dev`, `cmake --build --preset dev-debug`, `ctest --preset dev`
- MSVC: run `vcvars64.bat`, then configure with Ninja and the MSVC vcpkg toolchain
  (`x64-windows` triplet).
- LibreOffice check (a file that fails to load prints "source file could not be
  loaded"):
  `soffice.com -env:UserInstallation=file:///<scratch>/lo-profile --headless --convert-to txt:Text --outdir <scratch> tests/odt/out/*.odt`
