# Contributing

Contributing notes for the `docsmithcpp` project for both human and AI agents.

## Repository layout

This is a compiled C++ library

- [README](Readme.md)
- headers in ./include/docsmith
- source in ./src
- tests under ./tests
- examples under ./examples

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

## Design process

Ideas are discussed before code is written. If you are working with an
assistant, it should get confirmation before editing code, and is welcome to
critique a request and suggest alternatives.
