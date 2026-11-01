# Copyright 2025 Michael Coutlakis
#
# SPDX-License-Identifier: Apache-2.0
#
# lldb data formatters for the DocSmith++ document model.
#
# They hide the plumbing between a node and its content: the variant member
# `.v` of inline_node / block_node, the has_children base and its box<> around
# the child vector. A node shows its own fields (style, level, url) and then its
# children as [0], [1], ...; the summary line previews the node's text.
#
# Load it in lldb (or CodeLLDB's "initCommands") with:
#   command script import <repo>/tools/debug/docsmith_lldb.py

import lldb

PREVIEW_LEN = 60

# Nodes deriving from has_children<>, and the leaf fields each one shows.
CONTAINERS = {
    "paragraph": [],
    "heading": ["level"],
    "span": [],
    "hyperlink": ["target"],
    "list_item": [],
    "table_cell": ["rowspan", "colspan"],
}


def _short_name(valobj):
    name = valobj.GetType().GetUnqualifiedType().GetName()
    if name.startswith("docsmith::"):
        name = name[len("docsmith::"):]
    return name


def _synthetic(valobj):
    return valobj if valobj.IsSynthetic() else valobj.GetSyntheticValue() or valobj


def _string(valobj):
    """The contents of a std::string, unquoted."""
    s = valobj.GetSummary() or ""
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        s = s[1:-1]
    return s


def _style(valobj):
    """The style name of a styled<> node, or "" when it has none."""
    style = valobj.GetChildMemberWithName("style")
    if not style.IsValid():
        return ""
    return _string(style.GetNonSyntheticValue().GetChildMemberWithName("m_name"))


def _link_target(target):
    """A hyperlink's target variant: the URL, or "#" + the bookmark's name."""
    v = _synthetic(target)
    if v.GetNumChildren() == 0:
        return ""
    alt = v.GetChildAtIndex(0)
    url = alt.GetChildMemberWithName("url")
    if url.IsValid():
        return _string(url)
    return "#" + _string(alt.GetChildMemberWithName("name"))


def _alternative(node):
    """The active alternative held by an inline_node / block_node."""
    v = _synthetic(node.GetNonSyntheticValue().GetChildMemberWithName("v"))
    if v.GetNumChildren() == 0:
        return None
    return v.GetChildAtIndex(0)


def _children(node):
    """The child elements of a has_children<> node (or a list's items)."""
    raw = node.GetNonSyntheticValue()
    vec = raw.GetChildMemberWithName("m_items")
    if not vec.IsValid():
        vec = raw.GetChildMemberWithName("m_children")
        if vec.GetType().GetName().startswith("docsmith::box<"):
            vec = vec.GetChildMemberWithName("m_ptr").Dereference()
    if not vec.IsValid():
        return []
    vec = _synthetic(vec)
    return [vec.GetChildAtIndex(i) for i in range(vec.GetNumChildren())]


def _text(valobj, budget):
    """Plain text of a node, up to about `budget` characters."""
    name = _short_name(valobj)
    if name in ("inline_node", "block_node"):
        alt = _alternative(valobj)
        return _text(alt, budget) if alt else ""
    if name == "text":
        return _string(valobj.GetChildMemberWithName("content"))[:budget]
    out = ""
    sep = " " if name in ("list", "list_item", "table_cell") else ""
    for child in _children(valobj):
        if len(out) >= budget:
            break
        piece = _text(child, budget - len(out))
        out += (sep if out and piece else "") + piece
    return out


def _preview(valobj):
    s = _text(valobj, PREVIEW_LEN + 1)
    return '"' + (s[:PREVIEW_LEN] + "..." if len(s) > PREVIEW_LEN else s) + '"'


# ---- Summaries --------------------------------------------------------------

def node_summary(valobj, internal_dict):
    """inline_node / block_node: the summary of the alternative it holds."""
    alt = _alternative(valobj)
    if alt is None:
        return "<valueless>"
    return alt.GetSummary() or _short_name(alt)


def container_summary(valobj, internal_dict):
    name = _short_name(valobj)
    parts = [name]
    if name == "heading":
        parts.append(valobj.GetChildMemberWithName("level").GetValue())
    style = _style(valobj)
    if style:
        parts.append("[" + style + "]")
    if name == "hyperlink":
        parts.append("<" + _link_target(valobj.GetChildMemberWithName("target")) + ">")
    parts.append(_preview(valobj))
    return " ".join(parts)


def list_summary(valobj, internal_dict):
    parts = ["list", "size=%d" % len(_children(valobj))]
    style = _style(valobj)
    if style:
        parts.append("[" + style + "]")
    return " ".join(parts)


def table_summary(valobj, internal_dict):
    rows = _synthetic(valobj.GetChildMemberWithName("rows"))
    n = rows.GetNumChildren()
    cols = _synthetic(rows.GetChildAtIndex(0)).GetNumChildren() if n else 0
    header = valobj.GetChildMemberWithName("header_rows").GetValueAsUnsigned(0)
    return "table %dx%d" % (n, cols) + (", %d header" % header if header else "")


def text_summary(valobj, internal_dict):
    return valobj.GetChildMemberWithName("content").GetSummary()


def url_target_summary(valobj, internal_dict):
    return "url " + (valobj.GetChildMemberWithName("url").GetSummary() or "")


def bookmark_target_summary(valobj, internal_dict):
    return "to bookmark " + (valobj.GetChildMemberWithName("name").GetSummary() or "")


def bookmark_summary(valobj, internal_dict):
    return "bookmark " + (valobj.GetChildMemberWithName("name").GetSummary() or "")


def style_name_summary(valobj, internal_dict):
    return valobj.GetChildMemberWithName("m_name").GetSummary()


def box_summary(valobj, internal_dict):
    ptr = valobj.GetNonSyntheticValue().GetChildMemberWithName("m_ptr")
    if ptr.GetValueAsUnsigned() == 0:
        return "<empty>"
    target = ptr.Dereference()
    return target.GetSummary() or target.GetValue() or ""


def text_doc_summary(valobj, internal_dict):
    body = _synthetic(valobj.GetChildMemberWithName("body"))
    return "text_doc body size=%d" % body.GetNumChildren()


# ---- Synthetic children -----------------------------------------------------

class ForwardProvider:
    """Base: show a fixed list of (name, value) pairs as the children."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.items = []

    def num_children(self):
        return len(self.items)

    def get_child_index(self, name):
        for i, (n, _) in enumerate(self.items):
            if n == name:
                return i
        return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= len(self.items):
            return None
        name, value = self.items[index]
        addr = value.GetLoadAddress()
        if addr != lldb.LLDB_INVALID_ADDRESS:
            return self.valobj.CreateValueFromAddress(name, addr, value.GetType())
        return self.valobj.CreateValueFromData(name, value.GetData(), value.GetType())

    def has_children(self):
        return bool(self.items)


class NodeProvider(ForwardProvider):
    """inline_node / block_node: the alternative's children, without `.v`."""

    def update(self):
        alt = _alternative(self.valobj)
        self.items = []
        if alt is None or _short_name(alt) == "text":  # the summary says it all
            return False
        alt = _synthetic(alt)
        self.items = [(alt.GetChildAtIndex(i).GetName(), alt.GetChildAtIndex(i))
                      for i in range(alt.GetNumChildren())]
        return False


class ContainerProvider(ForwardProvider):
    """has_children<> nodes and list: own fields, then the children as [i]."""

    def update(self):
        name = _short_name(self.valobj)
        raw = self.valobj.GetNonSyntheticValue()
        self.items = []
        if _style(raw):
            self.items.append(("style", raw.GetChildMemberWithName("style")))
        for field in CONTAINERS.get(name, []):
            value = raw.GetChildMemberWithName(field)
            if field.endswith("span") and value.GetValueAsSigned() == 1:
                continue
            self.items.append((field, value))
        for i, child in enumerate(_children(raw)):
            self.items.append(("[%d]" % i, child))
        return False


class BoxProvider(ForwardProvider):
    """box<T>: the held value's children, without m_ptr / m_vtable."""

    def update(self):
        ptr = self.valobj.GetNonSyntheticValue().GetChildMemberWithName("m_ptr")
        self.items = []
        if ptr.GetValueAsUnsigned() == 0:
            return False
        target = _synthetic(ptr.Dereference())
        self.items = [(target.GetChildAtIndex(i).GetName(), target.GetChildAtIndex(i))
                      for i in range(target.GetNumChildren())]
        return False


# ---- libc++ vector with __layout_ ---------------------------------------------
#
# libc++ 23 moved std::vector's pointers into a `__layout_` member. lldb's own
# vector formatter only learned this in lldb 23, so older ones (CodeLLDB 1.12
# bundles lldb 22) show such a vector as empty. This fallback applies only to
# vectors with that layout and is harmless with an lldb that already knows it.

def is_layout_vector(sbtype, internal_dict):
    t = sbtype.GetCanonicalType()
    if not t.GetName().startswith("std::__1::vector<") or t.GetName().startswith(
            "std::__1::vector<bool,"):
        return False
    return any(t.GetFieldAtIndex(i).GetName() == "__layout_"
               for i in range(t.GetNumberOfFields()))


class LayoutVectorProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.size = 0

    def update(self):
        layout = self.valobj.GetNonSyntheticValue().GetChildMemberWithName("__layout_")
        begin = layout.GetChildMemberWithName("__begin_")
        end = layout.GetChildMemberWithName("__end_")
        self.type = begin.GetType().GetPointeeType()
        self.begin = begin.GetValueAsUnsigned()
        stride = self.type.GetByteSize()
        self.size = (end.GetValueAsUnsigned() - self.begin) // stride if stride else 0
        return False

    def num_children(self):
        return self.size

    def get_child_index(self, name):
        try:
            return int(name.lstrip("[").rstrip("]"))
        except ValueError:
            return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= self.size:
            return None
        return self.valobj.CreateValueFromAddress(
            "[%d]" % index, self.begin + index * self.type.GetByteSize(), self.type)

    def has_children(self):
        return self.size > 0


def layout_vector_summary(valobj, internal_dict):
    return "size=%d" % _synthetic(valobj).GetNumChildren()


# ---- Registration -----------------------------------------------------------

def __lldb_init_module(debugger, internal_dict):
    m = __name__
    containers = "|".join(CONTAINERS)
    commands = [
        f'type summary add -e -w docsmith -x "^docsmith::(inline|block)_node$" -F {m}.node_summary',
        f'type synthetic add -w docsmith -x "^docsmith::(inline|block)_node$" -l {m}.NodeProvider',
        f'type summary add -e -w docsmith -x "^docsmith::({containers})$" -F {m}.container_summary',
        f'type synthetic add -w docsmith -x "^docsmith::({containers}|list)$" -l {m}.ContainerProvider',
        f'type summary add -e -w docsmith "docsmith::list" -F {m}.list_summary',
        f'type summary add -e -w docsmith "docsmith::table" -F {m}.table_summary',
        f'type summary add -w docsmith "docsmith::text" -F {m}.text_summary',
        f'type summary add -w docsmith "docsmith::bookmark" -F {m}.bookmark_summary',
        f'type summary add -w docsmith "docsmith::url_target" -F {m}.url_target_summary',
        f'type summary add -w docsmith "docsmith::bookmark_target" -F {m}.bookmark_target_summary',
        f'type summary add -w docsmith "docsmith::style_name" -F {m}.style_name_summary',
        f'type summary add -e -w docsmith "docsmith::text_doc" -F {m}.text_doc_summary',
        f'type summary add -e -w docsmith -x "^docsmith::box<.+>$" -F {m}.box_summary',
        f'type synthetic add -w docsmith -x "^docsmith::box<.+>$" -l {m}.BoxProvider',
        f'type summary add -e -w docsmith --recognizer-function {m}.is_layout_vector -F {m}.layout_vector_summary',
        f'type synthetic add -w docsmith --recognizer-function {m}.is_layout_vector -l {m}.LayoutVectorProvider',
        "type category enable docsmith",
    ]
    for c in commands:
        debugger.HandleCommand(c)
