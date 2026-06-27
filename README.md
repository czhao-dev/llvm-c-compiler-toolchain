# c-llvm-toolchain

[![build-tool CI](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml/badge.svg)](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A small C toolchain built from scratch, one piece at a time: a preprocessor, a compiler, a static analyzer, a style linter, and a build tool.

Each subproject is independent and self-contained — its own language, build system, tests, and README — but together they cover the path from source code to a finished build: check the code, compile it, build it.

---

## Projects

| Project | Language | Description |
|---|---|---|
| [c-preprocessor](c-preprocessor) | C++17 | A minimal C preprocessor: `#include` file inclusion, object-like `#define`/`#undef` macros with hide-set-safe recursive expansion, and `//`/`/* */` comment stripping. Function-like macros, conditional compilation, and `##`/`#` are explicit non-goals — each is a hard error rather than a silent no-op. |
| [c-compiler-llvm](c-compiler-llvm) | C++17 / LLVM | **MiniC** — a compiler for a statically-typed subset of C. Hand-written lexer, recursive-descent parser, semantic analyzer, and LLVM IR codegen producing native binaries, cross-validated against clang. |
| [c-static-analyzer](c-static-analyzer) | Rust | A lightweight static analyzer for C code. Parses `.c`/`.h` files with tree-sitter (no compilation needed) and reports diagnostics for complexity, unused variables, nesting depth, missing returns, and unreachable code. |
| [c-linter](c-linter) | C++17 | A style/formatting linter for C: snake_case naming, line length (80 cols) and trailing whitespace, magic-number detection in comparisons, and K&R/Allman brace-style consistency. Reporting only — no auto-fixing, and no semantic checks (that's c-static-analyzer's job). |
| [build-tool](build-tool) | C++17 | A dependency-graph-aware build tool implementing core GNU Make semantics. Resolves a Makefile into a topologically-ordered plan, checks mtime-based staleness, and executes recipes serially with cycle detection and `-k`/`--keep-going` support. |

## Highlights

**c-preprocessor** — A four-stage pipeline (comment stripper → directive/include line-driver → tokenizer → hide-set-based macro rescanner) built on `libpp_core`, with zero external dependencies. Recursive macro expansion (a macro's replacement can reference another macro) terminates correctly on self-referential and mutually recursive definitions via the standard "blue paint" hide-set algorithm, matching real `cpp` behavior rather than erroring out. Circular `#include`s are detected and reported with the full chain; diamond includes are intentionally left un-deduplicated, since there are no include guards. All 7 test suites pass, including a byte-for-byte golden-output comparison and a subprocess-level exercise of the CLI.

**MiniC compiler** — A complete four-stage pipeline (lexer → recursive-descent parser → semantic analyzer → LLVM IR generator) compiled into `libminic_core.a` behind a thin CLI. All five test suites pass, and all nine example programs produce byte-for-byte identical output to `clang` on the same source. `-O1`/`-O2`/`-O3` run LLVM's real optimization pipeline (`mem2reg`, `instcombine`, `tailcallelim`, loop unrolling); `-O2` gets `fibonacci(40)` to a measured 1.4× speedup over `-O0`.

**C static analyzer** — Five rules (`SA001`–`SA005`) covering cyclomatic complexity, unused variables, nesting depth, missing returns, and unreachable code, built on tree-sitter so no compilation step is required. 44 tests pass — 36 unit tests plus 8 integration tests including a byte-for-byte golden-output comparison. Ships as a single self-contained binary with a CI-friendly non-zero exit code on findings.

**c-linter** — Five rules (`CL001`–`CL005`) covering snake_case naming, line length, trailing whitespace, magic numbers in comparisons, and K&R/Allman brace-style consistency, built on a small hand-written lexer that's deliberately never shared with `c-compiler-llvm`, keeping the subproject independent per this repo's convention. The lexer is tolerant by design — unterminated comments/literals and unmodeled punctuation never cause an error, since a linter has to process real-world C it doesn't fully model. All 7 test suites pass. Reporting only, with a CI-friendly non-zero exit code on findings.

**build-tool** — Parses a Makefile into rules, resolves them into a dependency graph with cycle detection and memoization (a shared prerequisite builds exactly once), skips up-to-date targets via mtime staleness, and runs outstanding recipes serially in topological order with `-k`/`--keep-going` support. `-j` parallelism is a deliberate non-goal: it only affects wall-clock build speed, not correctness, so this small tool trades it for a much simpler single-threaded executor. 22 tests pass, and it's the only subproject with CI wired up so far (path-scoped GitHub Actions workflow, gated on a CMake build and `ctest`).

## Getting Started

Each project builds independently — see its README for details.

```bash
# c-preprocessor (C++17/CMake, no external dependencies)
cd c-preprocessor && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure

# MiniC compiler (C++17/CMake, requires LLVM 17+)
cd c-compiler-llvm && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure

# C static analyzer (Rust)
cd c-static-analyzer && cargo build --release && cargo test

# c-linter (C++17/CMake, no external dependencies)
cd c-linter && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure

# build-tool (C++17/CMake, no external dependencies)
cd build-tool && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure
```

## References

Each subproject cites the sources specific to its own stage of the pipeline
in its own README — see
[c-preprocessor](c-preprocessor/README.md#references),
[c-compiler-llvm](c-compiler-llvm/README.md#references),
[c-static-analyzer](c-static-analyzer/README.md#references), and
[c-linter](c-linter/README.md#references). At the
toolchain level:

- ISO/IEC 9899:2018. *Programming Languages — C* (C17 standard) — the
  normative reference for the preprocessing, compilation, and
  translation-unit semantics these subprojects implement pieces of.
- Kernighan, Brian W. and Ritchie, Dennis M. *The C Programming Language*
  (2nd ed.). Prentice Hall, 1988. — the canonical description of the
  language this toolchain processes end to end.
- Aho, Lam, Sethi, Ullman. *Compilers: Principles, Techniques, and Tools*
  (2nd ed., "Dragon Book"). Addison-Wesley, 2006. — the standard reference
  for the overall shape of a toolchain pipeline: preprocess, compile,
  analyze, build.
- Levine, John R. *Linkers and Loaders*. Morgan Kaufmann, 2000. — companion
  reading for what happens just past this toolchain's own scope, once
  `build-tool` hands compiled objects off to the linker.

## License

Each subproject is MIT licensed; see the `LICENSE` file in this directory and in each subproject.
