# Design

DocSmith++ reads documents into one format-agnostic tree, lets C++ code build,
search and edit it, and writes it to any format. Each format is a reader and/or
a writer of that tree; nothing format-specific lives in it, except the
pass-through `unknown` node and the loaded file kept by a format's `file` class
(see *Formats*).

## Document model

The document tree is a pair of value-type variants — `inline_node` (text-flow
content: `text`, `span`, `hyperlink`, `frame`, `bookmark`, `field`, `unknown`) and
`block_node` (`paragraph`, `heading`, `list`, `table`, `frame`, `page_break`,
`table_of_contents`, `unknown`). It
is deliberately a _generic_, format-agnostic model: which group may go where is
encoded by the child sequence's type, so there is no separate per-format
validation layer. Copy and `operator==` are compiler-generated, and traversal is
`std::visit`. Shared behaviour lives in thin, non-polymorphic CRTP mixins:
`styled<Derived>` (a style name + `set_style`) and `has_children<…>`
(`children()`/`add()`).

An `unknown` node stores verbatim XML for anything the parser does not model, so
foreign content survives a parse → write round trip. Only block- and inline-level
content is kept this way: unrecognised children _inside_ a table or list, and
unknown attributes on known elements, are dropped (TODO.md, *Correctness*).

Content the model doesn't hold is either modelled or dropped, all or nothing:
what can't be modelled isn't kept half-modelled, since raw markup carried over
next to a node that is edited can end up contradicting it (a number cell's
value and its text). Where ODF content goes today, case by case, is recorded in
`tests/odt/test_odt_loss.cpp`.

The recursive edges (a `span` contains inline nodes, which contain spans…) are
held through a small value-semantic wrapper, `box<T>`
(`include/docsmithcpp/box.h`), which keeps the model portable across libstdc++,
libc++ and the MSVC STL.

`text_doc::operator==` compares everything: the body and the style, list style
and resource registries. To compare content only, compare `a.body == b.body`.

### Nodes

What each node holds, independent of any one format. "Inline" is a sequence of
`inline_node`, "blocks" a sequence of `block_node`.

| Node | Group | Holds | Notes |
|---|---|---|---|
| `text_doc` | root | blocks (`body`) | plus `meta`, `page` and the style, list style and resource registries |
| `page_style` | — | blocks (`header`, `footer`) | what each page repeats above and below the body |
| `paragraph` | block | inline | styled |
| `heading` | block | inline | styled; `level` |
| `list` | block | `list_item`s | styled with a list style; `continue_numbering` |
| `list_item` | — | blocks | `numbered`, `start_value` |
| `table` | block | a grid of `table_cell`s | styled; always rectangular; per row a style, per column a width and a cell style; the first `header_rows()` rows are header rows; `accessibility_title`, `accessibility_description` |
| `table_cell` | — | blocks | styled; `props` (direct formatting); `rowspan`, `colspan`; a covered slot holds a placeholder cell |
| `frame` | block, inline | an `image` | styled; `width`, `height`; `accessibility_title`, `accessibility_description` |
| `page_break` | block | — | starts a new page |
| `table_of_contents` | block | — | `title`, `levels`; the entries are made by writers |
| `text` | inline | a string | white space as is, `'\t'` a tab, `'\n'` a line break |
| `span` | inline | inline | styled |
| `hyperlink` | inline | inline | `target`: a URL or a bookmark |
| `bookmark` | inline | — | `name` |
| `field` | inline | — | `type`: page number, page count, date or title |
| `unknown` | block, inline | verbatim XML | another format's markup, passed through |

### Rules

The types enforce what may contain what; the rest are rules that writers rely
on.

- **Containment.** A paragraph or heading holds only inline content, so it
  can't hold a table or another paragraph. Blocks nest only through
  `list_item`, `table_cell` and the page header and footer, so a list in a list
  item, or a table in a table cell, is fine. Inline content nests through
  `span` and `hyperlink`. A `frame` is a block when it stands on its own and
  inline when it sits in a line of text.
- **Frames** are in the line of text, like a character: ODT `as-char`, DOCX
  `wp:inline`. A block frame is an unstyled paragraph holding only the frame,
  and reads back as a block frame. Anchoring a frame elsewhere (to a paragraph
  or page), its position and how text wraps round it aren't modelled, and are
  dropped when read, so a floating picture comes back in the line at its
  anchor (TODO.md, *Model gaps*).
- **Alternative text.** A frame or table can have an `accessibility_title` and
  `accessibility_description`: for screen readers and accessible PDF, never
  shown. ODT writes them as `svg:title` / `svg:desc` in the frame and
  `table:title` / `table:desc` in the table, DOCX as `wp:docPr` `title` /
  `descr` and `w:tblCaption` / `w:tblDescription`, HTML as `alt` on images and
  `aria-label` on tables (the title, else the description), Markdown as an
  image's alt text. A visible caption is a paragraph of its own.
- **List numbering.** A list's count starts at its list style's start for that
  level, or with `continue_numbering` carries on from the last list before it
  with the same list style at the same depth. An item's `start_value` restarts
  the count there; later items count on from it. An item that isn't `numbered`
  shows no number or bullet and isn't counted. Only ODF has such an item (as
  `text:list-header`), and only first, so writers write a later one as more of
  the item before it, as LibreOffice saves one, and Markdown and DOCX write one
  at the start as paragraphs before the list. HTML writes each as it is.
- **Tables are rectangular.** Every row has `num_cols()` cells. A cell with a
  `rowspan` or `colspan` covers the slots to its right and below, which still
  hold placeholder cells that writers skip (`is_cell_covered()`).
  `table::add_row()` checks the width, and `is_rectangular()` checks the whole
  table. `header_rows` counts rows from the top.
- **Heading levels** start at 1. A format with fewer levels clamps them (HTML
  and Markdown stop at 6).
- **Styles are referred to by name.** A node's style names a definition in
  `text_doc::styles()` or `list_styles()`, or a `builtin` style. Nothing checks
  that the name is defined (TODO.md, *Model gaps*). Writers write the name as
  given, so an undefined one gets whatever the output format gives it: an ODT
  application's own style of that name, or an HTML class with no rule.
- **Values.** Copying a node copies its whole subtree, and `==` compares
  structure. Image bytes in `resources()` are shared and read-only, so copying a
  document doesn't copy them.
- **What belongs where.** Anything more than one format needs (styles, images,
  metadata, the page header and footer) lives in `text_doc`. A format's `file`
  class holds only what is specific to that format: for ODT, the rest of the
  loaded package.

### Traversal

- **One level:** `std::visit` on a node's `v`, with the `overloaded` helper
  for one lambda per node type.
- **The whole tree:** `walk(doc, f)` calls `f` on every node, depth first, as
  `const` or not as `doc` is: the body first, then the page header and footer.
  On top of it, `collect<T>(doc)` returns pointers to every `T`, and
  `find_all<T>(doc, pred)` those that match. `strip_styles(doc)` clears every
  style name (`include/docsmithcpp/visit.h`).
- **Printing:** `io_writer` prints the tree (`include/docsmithcpp/iostream_writer.h`).
  The debugger visualisers in `tools/debug/` show a node's summary and children
  (see CONTRIBUTING.md, *Debugging*).

### White space and breaks

`text` holds text as it reads. Spaces are kept as they are, a tab is `'\t'` and
a line break is `'\n'`, so `paragraph("line 1\nline 2")` is two lines and
plain-text extraction needs nothing special. They aren't nodes of their own:
ODF's `text:s` exists only because XML collapses white space, so a node for it
would be a second way to write the same text, and `operator==` would tell the
two apart.

- **ODT**: the writer writes a space as is only where ODF would keep it (one,
  between two other characters); other spaces become `text:s`, `'\t'`
  `text:tab` and `'\n'` `text:line-break`. The parser reads them back into the
  text around them, and reads white space in the XML as ODF does: each run is
  one space, and it's dropped at the start of a paragraph.
- **HTML**: `'\n'` is `<br>`, a run of spaces keeps all but one as `&nbsp;`,
  and a tab is kept with `white-space: pre`.

A page break is a block, `page_break`, so it can be put in the body from code:
`doc.add(page_break{})`. ODF has no page break element: a paragraph style with
`fo:break-before` starts the page. The ODT writer folds a `page_break` into the
next paragraph or heading, giving it an automatic style made from its own
(a copy of an automatic style, or a child of a common one) with the break
added, named `<style>.PageBreak`. If no paragraph or heading follows (the end,
a table, an image), it writes an empty paragraph with a break (`PageBreakOnly`).
The parser turns those styles back into `page_break` nodes, so they aren't
loaded into `styles()`. A file edited in LibreOffice has its automatic styles
renamed, and its breaks then stay what they are in ODF, a property of the
paragraph's style. HTML writes `<div style="break-after: page"></div>`.

The properties `break_before` / `break_after` on a paragraph style still
describe a break that belongs to a style, e.g. every chapter heading.

### Filling in a template

A template is a document with placeholders: blocks that stand for content
filled in from code. `text_doc::replace()`, `insert_after()` and
`insert_before()` find one and put another document's body there:

    auto f = odt::file::load("report_template.odt");
    f.doc.replace(text{"{{summary}}"}, {heading(1, "Summary"), results});
    f.doc.insert_after(bookmark{"details"}, details);

A `placeholder` is one of the node types it is made of, so there is nothing new
to learn: a `bookmark` (a paragraph or heading holding it), a `style_name` (a
paragraph or heading with that style) or a `text` (a paragraph or heading whose
text is that, ignoring white space around it). The first match is used, looked
for depth first in the body, including lists and table cells, then in the page
header and footer. A placeholder is a whole block, because what replaces it is
usually several blocks (a heading and a table); text within a paragraph is
edited in place. The content is a `text_doc`, so a single node or a braced list
of nodes converts to it, and its styles and resources come with it, as with
`append()`.

### Pages, headers and footers

The model has no pages: the body is one flow, and formats that have pages lay
it out. What every page repeats is the document's `page`, a `page_style` with a
`header` and a `footer`, each a sequence of blocks (empty for none). It is
separate from the body because it isn't read in order with it, and a format
without pages still has a place for it. `walk()` and the searches built on it
visit the header and footer after the body.

    doc.page.set_header(paragraph(field{field_type::title}));
    doc.page.set_footer(paragraph("Page ", field{field_type::page_number}));

There is one page style per document, which is what a test report needs.
Several (a landscape page, a different first page) would be a registry of page
styles and a way to start one in the body, as ODF's master pages and DOCX's
sections do; page size and margins would go in `page_style` too.

- **ODT**: the header and footer are `style:header` / `style:footer` on the
  master page the body uses, "Standard" (LibreOffice's Default Page Style) if
  there is one, else the first. The writer replaces their content in place and
  keeps the rest of `styles.xml`: left and first-page headers, page layouts and
  other master pages. A new document gets a master page (A4, 2cm margins) only
  when it has a header or footer. Header text uses common styles, or the
  automatic styles in `styles.xml`, not `content.xml`'s.
- **HTML**: `<header>` before the body and `<footer>` after it, when not empty.

### Table of contents

A `table_of_contents` lists the body's headings down to `levels`, each linking
to its heading. It holds no entries: writers make them from the headings when
they write, so the list can't go stale as the body is edited or filled in.

- **ODT**: `text:table-of-content`, with entry templates for each level and an
  index body the writer fills in, because LibreOffice shows the stored entries
  and doesn't regenerate them when it opens a file. An entry links to the
  heading's first bookmark, else to `#<text>|outline`, LibreOffice's link to a
  heading. Page numbers need a layout, so entries have none until the index is
  updated in LibreOffice (Tools > Update). The writer defines Contents Heading
  and Contents 1-n as LibreOffice does unless they are defined. Reading keeps
  the title and levels and drops the stored entries.
- **HTML**: `<nav class="toc">` with the title and nested `<ul>`s of links.
  When a document has a table of contents, a heading with a bookmark is linked
  through it, and the others get an `id` made from their text (`test-results`,
  then `test-results-2`, ...).

### Fields

A `field` is a value the writer or reader fills in: the page number, the page
count, the date, or the document's title (`meta.title`). It goes anywhere text
goes, most usefully in a header or footer:
`paragraph("Page ", field{field_type::page_number}, " of ", field{field_type::page_count})`.

- **ODT**: `text:page-number`, `text:page-count`, `text:date` (as YYYY-MM-DD) and `text:title`,
  each holding the value it shows until the reader updates it: `1` for page
  numbers (the writer doesn't lay out pages), today's date and the title. Read
  back from the same elements; other ODF fields stay `unknown`.
- **HTML**: the title and today's date are written as text. HTML has no pages,
  so page numbers and counts are left out.

## Styles

A `style` is a named definition in `text_doc::styles()`; nodes refer to it by
name. Its family (`style_family`) says what kind of node it applies to, as ODF
and DOCX require: paragraph (`paragraph`, `heading`), text (`span`), graphic
(`frame`), and the table families. ODF names are unique per family. A style
built in code may leave the family empty: the ODT writer then works it out from
the nodes that use it, and writes it once for each family it's used with, so
the same style can be set on a span and on a paragraph. HTML has no families: a
style is a CSS class either way, and its rule includes what it inherits from its
parent styles.

- **ODT**: loading reads the definitions in `styles.xml` and `content.xml` into
  the registries, as far as the model holds their properties. On save, the writer
  starts from the loaded `styles.xml` and `content.xml` style containers. A
  definition that is unchanged since loading keeps its original XML, so the
  properties the model doesn't hold survive; an edited one replaces it in place;
  a new one is added (styles to `styles.xml`, list styles to `content.xml`'s
  automatic styles). Removing a definition from the registry doesn't remove it
  from the file, since nodes or `unknown` markup may still use it.

### Builtin styles and code blocks

Names in `builtin` are styles writers know without a definition in
`text_doc::styles()`: `title`, `preformatted` (a monospace paragraph for code
and logs) and `source_text` (monospace text within a line). They are
LibreOffice's own Title, Preformatted Text and Source Text, so ODT defines each
as LibreOffice does when the document uses it and neither the document nor the
loaded file defines it. HTML writes the class and leaves its look to
`html::file::css`, except that a preformatted paragraph is `<pre>` and a source
text span `<code>`, as is anything styled with a style based on them.

A code block is a paragraph, not a node of its own: its lines are separated by
`'\n'` in the text, and its white space is kept like any other text's (see
*White space and breaks*). So `paragraph(log).set_style(builtin::preformatted)`
is the whole of it, and a format without code blocks still gets a monospace
paragraph.

### Tables

A `table` is a grid: its cells are private, row by row, so every row has
`num_cols()` cells, and a slot spanned over by an earlier cell holds a
placeholder. Beside the cells it keeps a style per row and a relative width and
cell style per column. Content comes first and formatting after:

    table t = table::from_cells({{"Test", "Result"}, {"open", "pass"}});
    t.add_row({"save", "fail"});
    t.set_header_rows(1).set_style("Results").set_column_widths({3, 1});
    t.set_column_style(0, "Key"); // a table_cell style
    t.set_row_borders({0}, edge::bottom, border::solid(pt(1)));

Borders, shading, vertical alignment, padding and the paragraph style for the
cell's unstyled paragraphs are properties of a cell (`cell_props`). It is the
one way to say how cells look: in a cell style, as a cell's direct formatting,
or in a table style for the header rows (`header_cells`). No format holds a
paragraph style in a cell style, so writers put it on each of the cell's
paragraphs that has no style of its own. The formats store them differently: ODF has them only on cells,
DOCX on the table (outer edges and the lines inside) and on cells, HTML on
either. Keeping them on cells in the model means there is one place a border
can be, so nothing has to decide between a table's line and a cell's edge.

A cell's formatting comes from these layers, the later winning for each
property:

1. The table style's lines (`table_props`): the outer edges and the horizontal
   and vertical lines inside, and the cells' padding.
2. The column's style (`set_column_style()`), a cell style.
3. In a header row, the table style's `header_cells`. Its bottom line is also
   the top of the row below the header.
4. The cell's own style (`cell_props` in a `table_cell` style).
5. The cell's direct formatting, `table_cell::props`.

Layers 1 to 3 apply by position, so they are live: a row added later, or a
change to a style, is picked up when the document is written. Nothing is
copied into the cells. Word and LibreOffice also have conditional table styles
(first column, banded rows), but they hold few patterns and behave differently
across applications. Rules like those would be further position layers here,
written per cell.

The table's border setters (`set_borders`, `set_outer_border`,
`set_horiz_borders`, `set_vert_borders`, `set_row_borders`,
`set_column_borders`) and the `row()` / `col()` views write to the cells
instead (layers 4 and 5). They act once, on the cells there are: a row added
afterwards doesn't get them. A line between two cells is set on both, so
neighbours agree.

`effective_cell_props()` combines the layers for one cell; it is what writers
use, and what a caller can use to ask how a cell will look. The table's width
and alignment are in its style too.

- **ODT**: a table style is a `table` family style with the width and
  alignment. ODF can't hold the rest in it, so the writer gives each cell that
  needs one an automatic cell style with the layers combined, its own style
  (or else its column's) as parent. A column's style is its
  `table:default-cell-style-name`, and is written on its cells too. LibreOffice draws both cells' edges where two meet, so a line
  between two cells is written once, as the upper cell's bottom (or the earlier
  cell's end), with `none` on the other, as LibreOffice's own files have it.
  Columns with widths get automatic column styles; relative widths are written
  as given when they are whole numbers of LibreOffice's size, else scaled, since
  LibreOffice ignores small ones (`3*`). Reading puts borders back where the
  file had them, on cells (through their styles), and keeps the table's name,
  its style, row styles, column widths and columns' default cell styles (a
  cell with its column's style reads back unstyled). So a table style's lines don't
  survive in an `.odt` template.
- **DOCX**: a table style is a `w:style` of type `table` holding the outer and
  inside lines (`tblBorders`) and cell padding, as Word's own Table Grid does.
  DOCX has no cell styles, so a cell's style and direct formatting, its
  column's style and the header rows' formatting, are written as its `tcPr`. Reading does the reverse:
  `tblBorders` into the table style, `tcPr` into the cell's direct formatting.
- **HTML**: the table style is a class with the width and alignment and
  `border-collapse`. A cell's combined borders, background and padding are
  written in its `style` attribute, since CSS selectors for edges (first row,
  last column) break with spanned cells.
- **Markdown**: GFM tables have no formatting, so all of it is left out.

A cell's content is its blocks: there are no cell values or formulas. ODT's
`office:value-type`, `office:value` and `table:formula` are dropped when
reading, so a formula cell keeps the result it showed (TODO.md, _Model gaps_).

## Resources

Anything more than one format needs lives in `text_doc`, not in a format's file
class. Image bytes are such a thing, so the document holds them in
`resources()`: name → bytes and media type (`include/docsmithcpp/resource.h`).
The bytes are shared and read-only, so copying a document doesn't copy images,
and `resource::operator==` compares bytes, not pointers.

An image's `uri` is resolved in this order:

1. a resource of that name;
2. a file on disk;
3. left as is (e.g. a URL in HTML).

`text_doc::add_image()` adds the resource and returns the `image` in one step,
so the two can't be set up inconsistently. Writers only write resources the body
refers to; one whose image was removed is dropped.

- **ODT**: `load()` moves the images the tree refers to out of the package into
  the resources (no copy). `save()` embeds them from there. Entries that only
  `unknown` markup refers to stay in the package and are carried over.
- **HTML**: `save()` writes the resources under `resource_dir`, relative to the
  page, keeping their names, so a parsed `.odt`'s `Pictures/` ends up next to
  the page by default. Image files from disk are copied to
  `resource_dir/Pictures/` too, unless they're already inside `resource_dir`,
  so the output folder is self-contained.

## Metadata

Properties of the document as a whole live in `text_doc::meta`, a plain
`metadata` struct (`include/docsmithcpp/metadata.h`). Its fields are the ones
ODT, DOCX, PDF, HTML and Markdown front matter all have room for, mostly Dublin
Core: title, author, language, subject, description, keywords, created and
modified dates, plus `custom` name → value properties. It has no invariants, so
its fields are public.

Metadata is never drawn on the page. A visible title is a paragraph in the body
with the `builtin::title` style, so the two are set separately
(`doc.meta.title = …` and `doc.set_title(…)`). The title stays in the body,
rather than in a member of its own, because formats don't limit a document to
one title or put it first: a logo may come before it, and reading then saving a
file must keep it where it was. `find_title()` returns the first one, wherever
it is.

- `author` is one string, since only Markdown front matter holds a list; several
  authors go in it as e.g. `"A; B"`.
- Dates are `std::chrono::sys_seconds`, in UTC. ODT dates without a time zone
  (as LibreOffice writes them) are read as UTC.
- **ODT**: `meta.xml` maps `title`, `subject`, `description`, `language` to
  `dc:*`, `author` to `meta:initial-creator` (falling back to `dc:creator` when
  read), `keywords` to `meta:keyword`, `created` / `modified` to
  `meta:creation-date` / `dc:date` and `custom` to `meta:user-defined`. Saving
  updates the loaded `meta.xml` rather than replacing it, so the generator,
  statistics and editing time survive, and unchanged values keep their original
  form.
- **HTML**: `<html lang>`, `<title>` (`html::file::title`, else `meta.title`,
  else the first heading) and `author`, `description` and `keywords` meta tags.
- **Markdown**: not written. Front matter isn't part of CommonMark or GFM.

## Formats

Each format is a namespace with a `file` class: the document as its public
`doc` member, the format's own options, a static `load()` if it can be read,
and `save()` (plus `to_string()` for text formats).

| Format | Read | Write | Kept from a loaded file |
|---|---|---|---|
| ODT | `odt::file::load()` | `odt::file::save()` | the rest of the package: `styles.xml` and `meta.xml` are updated in place, settings, the thumbnail and pictures only `unknown` uses are carried over; `content.xml` is regenerated |
| DOCX | `docx::file::load()` | `docx::file::save()` | the rest of the package: `styles.xml`, `numbering.xml`, `settings.xml` and `docProps/core.xml` are updated in place, other parts carried over, direct formatting and image drawings written back as they were; `document.xml` is regenerated (see *DOCX*) |
| HTML | — | `html::file::save()`, `to_string()` | — |
| Markdown | `md::parse()`, `md::file::load()`, with `DOCSMITHCPP_MARKDOWN` | `md::file::save()`, `to_string()` | — |

- **Writers** are recursive `std::visit` emitters with one overload per node
  type and no catch-all, so a new node doesn't compile until every writer
  handles it. Writing never changes `doc`.
- **Readers** build the tree top-down and put anything they don't model in
  `unknown`, or drop it where `unknown` can't hold it (see *Document model*).
- **Round trips** go through the model: reading then writing the same format
  keeps what the model holds, plus what the `file` class carries over. Going
  to another format keeps only what the model holds.

## Source layout

Public headers are in `include/docsmithcpp/`. The model is `nodes_fwd.h` (forward
declarations and mixins), `inline_nodes.h`, `list.h`, `table.h`, `nodes.h`,
`page.h`, `style.h`, `resource.h`, `metadata.h` and `text_doc.h`. Traversal is in
`visit.h`, and each format's `file` class in `<format>/file.h`. `docsmith.h`
includes all of it. No public header includes a dependency's header.

Everything under `src/` is private:

| Path | What |
|---|---|
| `zip/` | a zip archive read from and written to bytes (miniz), knowing nothing of its contents |
| `odf/` | the ODF package (manifest, mimetype) and `meta.xml`, shared by the ODF formats |
| `odt/` | the ODT parser (`file.cpp`), writer and style XML |
| `opc/` | the OPC package of the Office Open XML formats: parts, content types, relationships |
| `xml/` | renaming namespace prefixes by URI, so readers don't depend on the prefixes a file declares |
| `docx/` | the DOCX reader, writer, style and numbering XML, and metadata |
| `html/`, `md/` | the HTML writer, and the Markdown writer and md4c parser |
| `outline.h` | plain text of inline content, the body's headings, and heading ids for a table of contents |
| `format.h` | number, colour and date formatting without a formatting library |
| `bytes.h` | whole files as bytes, through `std::filesystem::path` so non-ASCII names work on Windows |
| `image_size.h` | an image's pixel size and resolution from its PNG, JPEG or GIF header |
| `resource_plan.h` | where a text format's images go when it is saved with its images next to it |

## Extending

**A new node** touches every format, which is why the node list is short.

1. Define it in the header for its group and add it to `inline_node` or
   `block_node`. If it has children, `detail::deep_visitor` in `visit.h` has
   to recurse into them.
2. Handle it in every writer (`src/odt/writer.cpp`, `src/html/file.cpp`,
   `src/md/file.cpp`) and every reader that can produce it (`src/odt/file.cpp`,
   `src/md/parser.cpp`). The compiler finds the writers.
3. Show it in `io_writer`, `tools/debug/docsmith_lldb.py` and
   `tools/debug/docsmith.natvis`.
4. Add it to the table in *Nodes*, with a section here if its mapping to a
   format isn't obvious. Add tests for each format and a CHANGELOG entry.

Before adding a node, check whether text, a style or an existing node already
says it. A tab, a line break and a code block are all text and styles, not
nodes (see *White space and breaks*).

**A new format**:

- Add `include/docsmithcpp/<format>/file.h` and `src/<format>/`, with the
  `file` class shape above.
- Build zip-based formats on `src/zip/`. Read and write files through
  `src/bytes.h`, and share helpers through `src/outline.h` and `src/format.h`.
- Keep dependencies private, and optional where they can be
  (CONTRIBUTING.md, *Adding a dependency*).
- Give the format a section here with a mapping table like *Markdown*'s, and
  tests in `tests/<format>/`. Add it to the README's roadmap and to *Formats*
  above.

## Markdown

`md::file` writes GitHub Flavored Markdown with no dependency. `md::parse` and
`md::file::load` read it with md4c, and are built only with
`DOCSMITHCPP_MARKDOWN`. They live in their own namespace, like `odt::file` and
`html::file`, rather than on `text_doc`, so the core stays format-agnostic. A
parsed snippet goes into another document with `text_doc::append()`.

Markdown has fewer things than the model, and no styles at all, so the writer
maps what it can and keeps the content of the rest. The parser maps each
Markdown construct to the node and style a document built in code or loaded
from ODT would have, using LibreOffice's style names, so a parsed document
looks right in an `.odt` and writes back to the same Markdown.

| Model | Markdown | Read back as |
|---|---|---|
| `heading` | `#` to `######` (level clamped) | the same |
| `builtin::title` paragraph | `# ` | a level 1 heading |
| `paragraph` | a paragraph, its style dropped | the same, unstyled |
| span with a bold style | `**strong**` | span styled `Strong_20_Emphasis` (bold, defined in `styles()`) |
| span with an italic or oblique style | `*emphasis*` | span styled `Emphasis` (italic, defined) |
| span based on `builtin::source_text` | `` `code` `` | span styled `builtin::source_text` |
| other spans | their content | text |
| `builtin::preformatted` paragraph | fenced code block | the same; the info string is dropped |
| paragraph based on `Quotations` | `>` block quote | `Quotations` paragraphs |
| `'\n'` in text | hard break, `\` at the end of the line | `'\n'`; a soft break is a space |
| `hyperlink` | `[text](url)`, `<url>` when the text is the URL, `(#id)` for a bookmark | the same |
| `bookmark` | `<a id="name"></a>` | a bookmark |
| `list` | `-` or `1.` from its list style at that level, each item with its number (renderers take only the first item's) | list styled `List_20_1` or `Numbering_20_123[_start_N]` |
| `table` | GFM table | the same, see below |
| `frame` | `![alt](uri)`, the alt its accessibility title (else description), its size dropped; a block frame is a paragraph of its own | the alt as its accessibility title; a paragraph holding only an image is a block frame |
| `page_break` | thematic break `---` | `page_break` |
| `table_of_contents` | its title as a paragraph, then nested `-` lists of `[heading](#id)` links; headings without a bookmark get an `<a id>` anchor with the same slug HTML uses | the paragraph and lists |
| `field` | the title or date as text; page number and count left out | text |
| `text_doc::page` header and footer | left out: Markdown has no pages | — |
| `unknown` | left out (it holds another format's markup) | — |

- **Styles**: a span's style counts as bold or italic if it, or a style it is
  based on, says so. The names `Emphasis` and `Strong_20_Emphasis` count even
  without a definition, as builtin styles do. Other text properties (font, size,
  colour) have no Markdown form. Emphasis next to punctuation inside a word
  (`a**"b"**c`) doesn't open under CommonMark's flanking rules; that case isn't
  worked around.
- **Code blocks** lose their info string (```` ```c++ ````), since a code block
  is a paragraph; see TODO.md, *Model gaps*.
- **Escaping**: `\`, `` ` ``, `*`, `[`, `]`, `<`, `|` and `~` are always
  escaped, `_` at a word boundary (so `snake_case` stays as is), and `&` where it
  would read as an entity. At the start of a line, so are `#`, `>`, `-`, `+`,
  `=` and the `.` or `)` after a number; leading spaces and tabs become `&#32;`
  and `&#9;`, which would otherwise be dropped or start a code block. A URL
  written as text becomes a link when read back, since autolinks are on.
- **Lists** are tight (no blank lines) when every item is one plain paragraph,
  optionally followed by a list, and loose otherwise. Two lists of the same kind
  in a row are kept apart with an empty `<!-- -->` comment, which the parser
  drops. When parsing, a nested list of the same kind and start as its parent's
  style at that level stays unstyled, as nested lists from ODT do.
- **Tables**: GFM tables always have one header row. With `header_rows` of 1 or
  more the first row is the header (others are body rows); with 0 the header row
  is written empty, and the parser reads an all-empty header row back as
  `header_rows = 0`. A cell is one line: its paragraphs are joined with `<br>`,
  nested lists and tables become their text, and a spanned cell's content goes in
  its first slot, with the covered ones empty. The parser reads `<br>` back as
  `'\n'`.
- **Raw HTML** is dropped when parsing, apart from `<a id>` (or `<a name>`)
  anchors and `<br>`. Putting it in `unknown` would make the ODT writer write it
  into `content.xml` as XML.
- **Strikethrough** is parsed, but the model has no strikethrough, so only its
  text is kept. Task lists are not enabled, so `[ ]` stays text.
- **Images**: `save()` writes the resources an image refers to under
  `resource_dir`, and copies image files from disk, as HTML does. `load()`
  reads images given by a relative path that exists into `resources()`, so the
  document can be saved as `.odt` or `.html` elsewhere. `parse()` leaves image
  uris as written. Alt text and titles are dropped.

## DOCX

`docx::file` reads and writes WordprocessingML with no new dependency: the
package is an OPC zip (`src/opc/`) on the same private zip layer as ODT, with
`[Content_Types].xml` and relationship parts in place of ODF's manifest. Like
`odt::file`, it keeps the package it loaded, so load, edit, save writes back
the parts the model doesn't hold (theme, settings, footnotes, comments, fonts,
other headers) and updates `styles.xml`, `numbering.xml` and `settings.xml` in
place. Relationships, media, headers and numbering instances that nothing
refers to after writing are dropped.

pugixml matches names by prefix, not namespace, and a DOCX file may bind any
prefix (or the default namespace) to WordprocessingML. So every part is
normalised when it's parsed: each name is renamed to the prefix the code uses
for its namespace URI (`w:`, `r:`, `wp:`, ...; `src/xml/namespaces.h`), and
the declarations move to the root. Prefixes of namespaces the code doesn't know
are kept, renamed only if they clash, and `mc:Ignorable` follows them.

| Model | DOCX | Read back as |
|---|---|---|
| `paragraph` | `w:p`, its style as `w:pStyle` | the same; the default paragraph style reads as none |
| `heading` | `w:p` with `w:outlineLvl` (level - 1) and `w:pStyle` `Heading<level>`, or its own style | a paragraph with an outline level, directly or from its style; `Heading<level>` reads as none |
| `span` | runs with `w:rStyle`; runs don't nest, so a span in a span gets the inner style | runs of one style in one span |
| `text` | `w:t` (`xml:space="preserve"` where needed), `'\t'` `w:tab`, `'\n'` `w:br` | the same; `w:cr` is `'\n'` too |
| `hyperlink` | `w:hyperlink`, a URL as an external relationship, a bookmark as `w:anchor`; unstyled text gets the `Hyperlink` character style | the same, without the `Hyperlink` style |
| `bookmark` | `w:bookmarkStart` and `w:bookmarkEnd` | a bookmark; `_Toc...` ones are dropped |
| `list` | items are paragraphs with `w:numPr` (`w:ilvl` = depth); see below | lists, nested by `w:ilvl` |
| `table` | `w:tbl` on an even grid at full width, `w:gridSpan`, `w:vMerge`, `w:tblHeader` on the first `header_rows` rows; no borders | the same; cell, row and table properties are dropped |
| `frame` | `w:drawing/wp:inline`, alternative text as `wp:docPr` `title` and `descr`; a block frame is a paragraph of its own | a frame; an unstyled paragraph holding only an image is a block frame |
| `page_break` | a paragraph holding `w:br w:type="page"` | a page break; one inside a paragraph splits it, and `w:pageBreakBefore` on a paragraph is one before it |
| `table_of_contents` | see below | the title and levels |
| `field` | `w:fldSimple` `PAGE`, `NUMPAGES`, `DATE \@ "yyyy-MM-dd"`, `TITLE`, with the value it shows | those four, from simple or complex (`w:fldChar`) fields |
| `text_doc::page` | header and footer parts, the default ones of the body's `w:sectPr` | the same |
| `unknown` | written verbatim if it holds OOXML (`markup::ooxml`); ODF is left out | elements the reader doesn't model |

- **Styles**: a style's name is its `w:styleId` as is, so names round-trip
  exactly; `w:name` is the name with ODF's escapes decoded (`_20_` is a space).
  Paragraph styles are `w:type="paragraph"`, text styles `character`, table
  styles `table`; graphic and table column, row and cell styles have no DOCX
  form. A style with no family is written for each family it's used with; if
  that is both, the character one is `<name>Char`, linked with `w:link`. Font,
  size (half-points), weight, italic, colour and `break_before` page map to
  `w:rFonts`, `w:sz`, `w:b`, `w:i`, `w:color` and `w:pageBreakBefore`. As with
  ODT, an unchanged loaded style keeps its XML and an edited one replaces it.
  The writer defines the styles it uses that neither the document nor the file
  defines: the builtins (Title; Preformatted Text and Source Text in Courier
  New), `Heading1`-`9`, `Hyperlink` and the table of contents ones. When
  reading, Word's `HTML Preformatted` and `HTML Code`, and LibreOffice's names
  for its own, are the builtins.
- **Direct formatting**: Word puts formatting on the run or paragraph itself
  (`w:rPr`, `w:pPr`) where ODF makes an automatic style. The reader does what
  ODF does: each distinct direct formatting becomes a style, `T1`, `T2`, ... for
  runs and `P1`, ... for paragraphs, based on the run's or paragraph's own
  style, so the model sees bold text as bold. The file keeps the original XML,
  and while the definition is unchanged the writer writes that XML back instead
  of the style, so properties the model doesn't hold (underline, spacing,
  alignment, a section break) survive. Properties that repeat what the style
  already says, or set a default (as LibreOffice writes `w:jc="left"` on every
  paragraph), aren't direct formatting.
- **Lists**: each list style is a `w:abstractNum` named after it (`w:name`),
  with each level's format (`decimal`, `lowerLetter`, ..., `bullet`), text
  (prefix, `%<level>`, suffix), start and indent. Each `list` node gets a `w:num`
  of its own whose `w:startOverride`s restart it, unless it continues the
  numbering: then it uses the last list's `w:num` again. An item's
  `start_value` starts a new `w:num` with a `w:startOverride` at its level, which
  the rest of the list uses. Reading follows Word, which counts on through every
  `w:num` of an abstractNum unless one restarts a level the first time it's used
  there: a `w:num` of the same abstractNum carries the list on (with a
  `start_value` if it restarts), and a list that isn't nested continues the
  numbering if its level was counted before and its `w:num` doesn't restart it.
  Bulleted levels never continue. An unstyled nested list
  continues its parent's numbering one level down; a styled one gets its own.
  The second and later paragraphs of an item have `w:numId` 0 and the item's
  indent, so they read back into the item. A list with no list style gets
  bullets from an abstractNum named "DocSmith Default List", which reads back
  unstyled. An abstractNum with no name reads as `WWNum<id>`, as LibreOffice
  names them.
- **Images**: written to `word/media/` (a resource already named
  `word/media/...` keeps its name) and sized in EMUs from the frame's `width`
  and `height`, or with neither from the image's own pixel size and resolution
  (`src/image_size.h` reads PNG, JPEG and GIF headers), scaled down to the text
  width. Loading reads the size from `wp:extent` and names each resource by its
  part. The file keeps a loaded image's `w:drawing`, and writes it again
  (alt text, cropping, effects) while a frame in the same part shows that
  image, with the frame's size in place of the one it had. A floating one
  (`wp:anchor`) is kept as `wp:inline`, without its position and wrap (see
  *Rules*, Frames).
- **Table of contents**: a `w:sdt` content control (docPartGallery "Table of
  Contents"): the title in `TOCHeading`, then a `TOC \o "1-<levels>" \h \z \u`
  complex field whose result is a `TOC<level>` paragraph per heading, linking
  to the heading's first bookmark or to a `_Toc<n>` bookmark the writer adds.
  As in ODT the entries have no page numbers; `w:updateFields` in
  `settings.xml` has Word offer to update the fields when it opens the file.
  Reading takes the title and levels from a content control or a bare `TOC`
  field, and drops the entries.
- **Pages**: a loaded `w:sectPr` (page size, margins, first and even page
  headers) is kept, its default header and footer references replaced. A new
  document is A4 with 2cm margins.
- **Metadata**: `title`, `author`, `language`, `subject` and `description` are
  `dc:*` in `docProps/core.xml`, `keywords` is `cp:keywords` (joined with `, `,
  split on `;` or `,`), `created` and `modified` are `dcterms:*`, and `custom`
  is `docProps/custom.xml`. A loaded core.xml is updated in place.
