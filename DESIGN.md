# Design

## Document model

The document tree is a pair of value-type variants — `inline_node` (text-flow
content: `text`, `span`, `hyperlink`, `frame`, `bookmark`, `unknown`) and
`block_node` (`paragraph`, `heading`, `list`, `table`, `frame`, `unknown`). It
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
`text_doc::styles()`. ODT defines Title as LibreOffice does when the document
doesn't; HTML writes the class and leaves its look to `html::file::css`.

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
