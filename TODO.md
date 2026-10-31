# TODO

Outstanding work, roughly in priority order. Each item says why it matters and
where to start. See `CHANGELOG.md` for what's done.

## Next up

### 1. Load style definitions when parsing ODT
The parser keeps style *names* on nodes but not their *definitions*, so
`text_doc::styles()` / `list_styles()` are empty after `odt::file::load()`. This is
why `example_odt_to_html` on `tests/odt/moderate.odt` turns numbered lists into
bullets and emits classes with no CSS.

- Read common styles from `styles.xml` and automatic styles from `content.xml`
  (`office:automatic-styles`), including `text:list-style`, into the registries.
  It's the inverse of the `write_xml(...)` functions in `src/odt/writer.cpp`.
- **Watch out for duplicates.** On save, `writer::carry_over()` copies the original
  automatic styles into the new `content.xml`, *and* `write_doc()` writes the list
  styles from the registry. Once the registries are populated by parsing, the
  definitions would be written twice. Decide whether parsed styles stop being
  carried over or are de-duplicated by name.
- The writer always uses `style:family="paragraph"` (FIXME in `write_xml(style)`).
  Text styles on spans need `"text"`, so the style model needs to know its family.
- Common styles are written into `content.xml`'s `office:styles`. ODF puts them in
  `styles.xml`. LibreOffice accepts it, but it's not to spec.

### 2. Decide where image data lives
At the moment the image bytes live only in the ODT package (`odf::package`, inside
`odt::file`). HTML needs them too: a page converted from a parsed `.odt` has
`src="Pictures/..."`, a path inside a zip, so images don't show.

- Option A: a resource registry on `text_doc` (name → bytes and media type), like
  the style registries. `image::uri` names a resource. ODT load fills it, the ODT
  writer embeds from it, and the HTML writer copies the images next to the page or
  inlines them as `data:` URIs.
- Option B: keep resources per format, and convert explicitly between formats.
- This follows the rule used so far: anything more than one format needs goes in
  `text_doc`. That points to A.
- A planned `odt::file::add_resource()` was deliberately left out until this is
  decided.

### 3. Separate HTML serialisation from the document mapping
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
- Revisit the third-party route if HTML (or Markdown) *parsing* becomes a goal.
  Then one library covering both directions would earn its place.

### 4. Tag 0.1.0
The minimal working version (build a document in C++, write ODT and HTML, parse
ODT) works. Move `[Unreleased]` in `CHANGELOG.md` to `[0.1.0]` and tag it.

## Testing and CI
- **Optional LibreOffice validation test.** Headless LibreOffice found a bug every
  unit test missed (an `office:version` mismatch made it reject resaved files).
  Add a ctest that runs only when `soffice` is found and converts every file in
  `tests/odt/out/` (see *Verifying* below).
- **CI** (e.g. GitHub Actions) for msys2 clang64 and MSVC. **Linux (GCC and Clang)
  has never been built**, and should be part of CI.
- **Fixtures:** the old experimental samples were deleted. Add small, targeted
  `.odt` fixtures per feature (lists, figures, bookmarks, page breaks, nested
  tables) as tests need them.
- **Test `find_package(DocSmithCpp)`** from a separate consumer project, against an
  installed build. It has never been tried.

## Format and model gaps
- `unknown` round-trips only block- and inline-level content. Unrecognised children
  *inside* tables and lists, and unknown *attributes* on known elements, are
  dropped.
- `unknown` holds ODT XML but doesn't say which format it came from. Record the
  source format so writers for other formats can decide what to do with it.
- ODT inline helpers `text:s` (repeated spaces), `text:tab` and `text:line-break`
  currently become `unknown`, so extracting the plain text loses them. Model them,
  or map them to text.
- The parser flattens `table:table-header-rows` into ordinary rows, so header rows
  are lost (in both ODT output and HTML `<thead>`/`<th>`).
- `set_style(style)` keeps only the name; the definition must also be added to
  `text_doc::styles()`. This is easy to get wrong. Consider a helper on the
  document that does both.
- When a loaded file is saved, images that are no longer referenced stay in the
  package.
- `frame` holds only an image. ODF frames can also hold text boxes and objects.
- `text_doc`: `body` is a public member, but the registries are private with
  accessors. Make access consistent. `operator==` deliberately ignores the
  registries; keep that documented.
- HTML: list number prefixes and suffixes, and parent styles, aren't reflected in
  the CSS.
- Non-ASCII paths: libzippp gets `path::string()`, which on Windows is the ANSI
  code page, so Unicode file names may fail.

## Performance (deferred)
- `box<T>` costs one heap allocation per span, hyperlink, list item and table cell.
  The block side could be boxed per list or table instead
  (`list::items`, `table::rows`), leaving `list_item` and `table_cell` unboxed. The
  inline side (span, hyperlink) needs one box per instance.
- If allocation ever shows up in profiles, `box` is the natural place to add an
  arena or pool.

## Docs and project
- Write a short spec of the generic document model: which nodes exist and which may
  contain which, independent of any one format. It would guide new nodes and new
  formats.
- The README's "Typical use case scenarios" section is a placeholder.

## Verifying changes
- Build and test (msys2 clang64 via `CMakeUserPresets.json`):
  `cmake --preset dev`, `cmake --build --preset dev-debug`, `ctest --preset dev`
- MSVC: run `vcvars64.bat`, then configure with Ninja and the MSVC vcpkg toolchain
  (`x64-windows` triplet).
- LibreOffice check (a file that fails to load prints "source file could not be
  loaded"):
  `soffice.com -env:UserInstallation=file:///<scratch>/lo-profile --headless --convert-to txt:Text --outdir <scratch> tests/odt/out/*.odt`
