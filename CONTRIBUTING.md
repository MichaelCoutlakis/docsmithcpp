# Contributing

Contributing notes for the `docsmithcpp` project for both human and AI agents.

## Repository layout

This is a compiled C++ library

- [README](README.md)
- [DESIGN](DESIGN.md): document model and design decisions
- headers in ./include/docsmithcpp
- source in ./src
- tests under ./tests
- examples under ./examples
- debugger visualisers under ./tools/debug (see [Debugging](#debugging))

## Conventions

Use the std c++ naming conventions.

Further notes:

- `snake_case` for types, functions, and variables; members prefixed `m_`.
- `PascalCase` for template parameters and for the case name in
  `TEST(Suite, CaseName)` per GTest convention.
- Every file starts with the SPDX MIT header block.
- Public API documented with Doxygen `///` comments — only where the comment
  adds what the code does not already convey.
- Formatting is governed by `.clang-format`;

Choose names with brevity, meaning, and clarity.

## Debugging

A document is a tree of values, so the debugger can show all of it. Without
help, though, each level sits under plumbing: the node variant's `.v`, the
`has_children` base and the `box` around recursive children. The visualisers in
`tools/debug/` hide that plumbing. A node shows its summary (`heading 1 "Hello
World!"`, `paragraph [Note] "..."`), its own fields (style, level, url) and its
children as `[0]`, `[1]`, ...

- `docsmith_lldb.py`: lldb formatters for CodeLLDB, lldb-dap and the lldb
  command line. Load it with
  `command script import <repo>/tools/debug/docsmith_lldb.py`. It also fixes
  `std::vector` display (shown as empty) when lldb is older than the libc++ it
  debugs, as with CodeLLDB 1.12 (lldb 22) on msys2 clang64 (libc++ 23).
- `docsmith.natvis`: Visual Studio, and VS Code's `cppvsdbg`, for MSVC builds.
  It reads the MSVC STL's layouts, so with other standard libraries it shows
  less.

VS Code with clang (msys2 clang64, the `dev` preset) and CodeLLDB, in
`.vscode/launch.json`:

```json
{
    "type": "lldb",
    "request": "launch",
    "name": "Debug CMake Target",
    "program": "${command:cmake.launchTargetPath}",
    "cwd": "${workspaceFolder}",
    "initCommands": ["command script import ${workspaceFolder}/tools/debug/docsmith_lldb.py"]
}
```

For the CMake Tools debug button, put the same `initCommands` under
`cmake.debugConfig` in `.vscode/settings.json`. lldb-dap (`"type": "lldb-dap"`)
uses the same `initCommands`.

VS Code with MSVC (`cppvsdbg`):

```json
{
    "type": "cppvsdbg",
    "request": "launch",
    "name": "Debug (MSVC)",
    "program": "${command:cmake.launchTargetPath}",
    "cwd": "${workspaceFolder}",
    "visualizerFile": "${workspaceFolder}/tools/debug/docsmith.natvis"
}
```

In Visual Studio, add `docsmith.natvis` to the solution, or copy it to
`%USERPROFILE%\Documents\Visual Studio 18\Visualizers`.

In the lldb command line, a value's summary hides its children unless you ask
for depth, for example `frame variable -D 8 doc.body`.

## Design process

Ideas are discussed before code is written. If you are working with an
assistant, it should get confirmation before editing code, and is welcome to
critique a request and suggest alternatives.
