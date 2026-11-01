# Design

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
foreign content survives a parse → write round trip. Note: this currently
applies to block- and inline-level content only — unrecognized children _inside_
a table or list, and unknown attributes on known elements, are not yet captured.

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
| `list` | block | `list_item`s | styled with a list style |
| `list_item` | — | blocks | |
| `table` | block | rows of `table_cell`s | every row has `num_cols()` cells; the first `header_rows` rows are header rows |
| `table_cell` | — | blocks | styled; `rowspan`, `colspan`; a covered slot holds a placeholder cell |
| `frame` | block, inline | an `image` | styled |
| `page_break` | block | — | starts a new page |
| `table_of_contents` | block | — | `title`, `levels`; the entries are made by writers |
| `text` | inline | a string | white space as is, `'\t'` a tab, `'\n'` a line break |
| `span` | inline | inline | styled |
| `hyperlink` | inline | inline | `target`: a URL or a bookmark |
| `bookmark` | inline | — | `name` |
| `field` | inline | — | `type`: page number, page count, date or title |
| `unknown` | block, inline | verbatim XML | another format's markup, passed through |

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
| `list` | `-` or `1.` (with the start number) from its list style at that level | list styled `List_20_1` or `Numbering_20_123[_start_N]` |
| `table` | GFM table | the same, see below |
| `frame` | `![](uri)`; a block frame is a paragraph of its own | a paragraph holding only an image is a block frame |
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
