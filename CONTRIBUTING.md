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

- `snake_case` for types, functions, and variables; private members prefixed
  `m_`.
- Public fields: bare names on nodes, style types and list levels (`level`,
  `parent`, `para`, `start_from`); `m_` on the property value types
  (`text_props::m_font_size`, `length::m_points`, `space_after::m_length`).
  There the prefix lets a field share its name with its tag type, which C++
  doesn't allow unprefixed (`std::optional<font_size> font_size;`), so keep it
  for consistency across all property types.
- `PascalCase` for template parameters and for the case name in
  `TEST(Suite, CaseName)` per GTest convention.
- Every file starts with the SPDX header block.
- Public API documented with Doxygen `///` comments — only where the comment
  adds what the code does not already convey.
- Formatting is governed by `.clang-format`;

Choose names with brevity, meaning, and clarity.

## Adding a dependency

Dependencies are a cost to every consumer, so discuss one before adding it. The
current ones are pugixml and miniz, plus md4c for the optional Markdown input.
When one is added:

- Keep it private: no public header includes it, and it is linked `PRIVATE` in
  `src/CMakeLists.txt`. Add its target to `cmake/DocSmithCppDependencies.cmake`,
  so projects using DocSmith++ are told to find it (they find every dependency
  themselves), and to the README's list. It's a breaking change for them.
- Its licence must be compatible with Apache-2.0 (e.g. MIT, BSD, zlib,
  Apache-2.0).
- If it is Apache-licensed and ships a `NOTICE` file, add its attribution to
  [NOTICE](NOTICE). Other licences don't use `NOTICE`.
- If its source is vendored into this repository, keep its licence file next to
  it.
- If it comes from vcpkg, add it to `VCPKG_PACKAGES` in
  [.github/workflows/ci.yml](.github/workflows/ci.yml).

## Continuous integration

[.github/workflows/ci.yml](.github/workflows/ci.yml) builds and tests every
push to `main` and every pull request:

- Linux (libstdc++), with the newest compilers on the runner, GCC 14 and
  Clang 18, and the oldest the README supports, GCC 11 and Clang 14.
- Windows, with MSVC and the `x64-windows` triplet.

Tests, examples and Markdown input are included. The `consumer` test runs;
`libreoffice_opens_output` doesn't, as LibreOffice isn't installed.

Dependencies come from vcpkg in classic mode at the commit in `VCPKG_COMMIT`.
Their built binaries are cached, so they are only rebuilt when that commit,
`VCPKG_PACKAGES` or the compiler changes. Bump `VCPKG_COMMIT` to move to newer
dependency versions.

## The README's showcase

The code block in the README's *Showcase* section is what lies between the
`<snippet-start:showcase>` and `<snippet-end:showcase>` comments in
[examples/showcase.cpp](examples/showcase.cpp), so edit it there. Building `example_showcase` with `DOCSMITHCPP_SYNC_README` (on in
the `dev` preset) copies it into the README, after it has compiled. The
`readme_showcase_in_sync` test fails if the two differ, which is what catches a
change committed without it, so CI leaves the option off.

## Debugging

A document is a tree of values, so the debugger can show all of it. Without
help, though, each level sits under plumbing: the node variant's `.v`, the
`has_children` base and the `box` around recursive children. The visualisers in
`tools/debug/` hide that plumbing. A node shows its summary
(`heading 1 "Hello World!"`, `paragraph [Note] "..."`), its own fields (style,
level, url) and its children as `[0]`, `[1]`, ...

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
  "initCommands": [
    "command script import ${workspaceFolder}/tools/debug/docsmith_lldb.py"
  ]
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
