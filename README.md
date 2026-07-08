# c-llvm-toolchain

[![build-tool CI](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml/badge.svg)](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A small C toolchain built from scratch, one piece at a time: a preprocessor, a compiler, a static analyzer, a style linter, a build tool, and a linker.

Each subproject is independent and self-contained — its own language, build system, tests, and README — but together they cover the path from source code to a finished build: check the code, compile it, build it.

---

## Projects

| Project | Language | Description |
|---|---|---|
| [c-preprocessor](c-preprocessor/README.md) | C++20 | A minimal C preprocessor built as a four-stage pipeline (comment stripper → directive/include line-driver → tokenizer → hide-set-based macro rescanner) with zero external dependencies: `#include` file inclusion resolved relative to the including file plus `-I` search paths, object-like `#define`/`#undef` macros with correct recursive expansion via the classic hide-set ("blue paint") algorithm (so self-referential and mutually recursive macros terminate instead of looping), and `//`/`/* */` comment stripping that leaves line numbers intact. Circular includes are detected and reported with the full chain. Function-like macros, conditional compilation (`#ifdef`/`#if`), and `##`/`#` are explicit non-goals — each is a hard `file:line` error rather than a silent no-op. All 7 test suites pass, including a byte-for-byte golden-output comparison and a subprocess-level CLI exercise. |
| [c-compiler-llvm](c-compiler-llvm/README.md) | C++20 / LLVM | **MiniC** — a compiler for a statically-typed subset of C, taking source through a hand-written lexer, a recursive-descent parser, a semantic analyzer (undeclared-variable and type-mismatch checking, argument-count and return-type validation), and an LLVM IR generator built on `IRBuilder<>`, producing native binaries via the LLVM backend. Supports pointers, fixed-size arrays with decay, structs/unions/enums with whole-value assignment, the full arithmetic/bitwise/logical/ternary/compound-assignment operator set, `if`/`while`/`for`/`do`-`while`/`switch`/`goto`, and recursive functions. `-O1`/`-O2`/`-O3` run LLVM's real new-pass-manager pipeline (`mem2reg`, `instcombine`, `tailcallelim`, loop unrolling), taking `fibonacci(40)` to a measured 1.4× speedup over `-O0`. All 5 test suites pass, and all nine example programs produce byte-for-byte output identical to `clang` compiling the same source. |
| [c-static-analyzer](c-static-analyzer/README.md) | C++20 | A lightweight static analyzer for C code that parses `.c`/`.h` files with tree-sitter's C API and the `tree-sitter-c` grammar (fetched via CMake `FetchContent` and compiled as plain C static libraries — the only subproject with an external dependency) rather than compiling or executing anything. Five independent rules (`SA001`–`SA005`) cover cyclomatic-complexity thresholds, unused local variables, control-flow nesting depth, non-exhaustive return paths, and unreachable code after `return`/`break`/`continue`/`goto`, each reporting stable file:line diagnostics. Configurable via CLI flags or a `.c-static-analyzer.toml` file (rule selection, thresholds, exclude globs), with default-excluded build/vendor directories and a CI-friendly non-zero exit code on findings. 10 test suites pass, including a byte-for-byte golden-output comparison. |
| [c-linter](c-linter/README.md) | C++20 | A style/formatting linter for C built on a small hand-written lexer that's deliberately never shared with `c-compiler-llvm`, keeping the subproject independent per this repo's convention — and tolerant by design, so unterminated comments/literals and unmodeled punctuation never cause an error. Five rules (`CL001`–`CL005`): snake_case naming (per-occurrence, no symbol table), line length (80 cols, configurable) and trailing whitespace via a raw-text pass ahead of tokenization, magic-number detection in comparisons (with `0`/`1`/`-1` exempted as common sentinels), and K&R/Allman brace-style consistency for `if`/`while`. Reporting only — no auto-fixing, no indentation tracking, and no semantic checks (that's `c-static-analyzer`'s job). All 7 test suites pass, with CI-friendly exit codes (`0`/`1`/`2`). |
| [build-tool](build-tool/README.md) | C++20 | A dependency-graph-aware build tool implementing core GNU Make semantics from scratch. Parses a Makefile into target/prerequisite rules, resolves them into a dependency graph via a single recursive depth-first walk that both detects cycles and produces a valid topological order (with memoization, so a shared prerequisite builds exactly once), checks mtime-based staleness to skip up-to-date targets, and executes outstanding recipes serially with fail-fast or `-k`/`--keep-going` semantics. Supports explicit rules, `.PHONY` targets, and a default goal; variable expansion, pattern rules, and `-j` parallelism are deliberate non-goals (parallelism only affects wall-clock speed, not correctness, for a tool this size). 22 tests pass across parser/planner/end-to-end suites, and it's the only subproject with CI wired up so far (path-scoped GitHub Actions workflow gated on a CMake build and `ctest`). |
| [c-linker](c-linker/README.md) | C++20 | A static linker for real ELF64 x86-64 relocatable object files (`ET_REL`, the same output `clang -c` produces) — parsed via pointer-casting onto a small vendored subset of the ELF64 structures rather than a toy format. Merges `.text`/`.data` sections across input files with per-file alignment padding, resolves symbols with undefined-symbol and multiple-definition detection, patches `Abs64`/`Pc32`/`Plt32` relocation sites using the real x86-64 psABI formulas, and emits a real, minimal, loadable static ELF executable (two page-aligned `PT_LOAD` segments) runnable directly on x86-64 Linux. No dynamic linking (no PLT/GOT), archive (`.a`) parsing, or link-time optimization. All 6 test suites pass, every one exercising real `.o` files compiled on the fly by `clang` rather than checked-in binary fixtures, independently verified with `readelf`. |

## Highlights

**c-preprocessor** — A four-stage pipeline (comment stripper → directive/include line-driver → tokenizer → hide-set-based macro rescanner) built on `libpp_core`, with zero external dependencies. Recursive macro expansion (a macro's replacement can reference another macro) terminates correctly on self-referential and mutually recursive definitions via the standard "blue paint" hide-set algorithm, matching real `cpp` behavior rather than erroring out. Circular `#include`s are detected and reported with the full chain; diamond includes are intentionally left un-deduplicated, since there are no include guards. All 7 test suites pass, including a byte-for-byte golden-output comparison and a subprocess-level exercise of the CLI.

**MiniC compiler** — A complete four-stage pipeline (lexer → recursive-descent parser → semantic analyzer → LLVM IR generator) compiled into `libminic_core.a` behind a thin CLI. All five test suites pass, and all nine example programs produce byte-for-byte identical output to `clang` on the same source. `-O1`/`-O2`/`-O3` run LLVM's real optimization pipeline (`mem2reg`, `instcombine`, `tailcallelim`, loop unrolling); `-O2` gets `fibonacci(40)` to a measured 1.4× speedup over `-O0`.

**C static analyzer** — Five rules (`SA001`–`SA005`) covering cyclomatic complexity, unused variables, nesting depth, missing returns, and unreachable code, built directly against tree-sitter's C API and the `tree-sitter-c` grammar (fetched via CMake `FetchContent` and compiled as plain C static libraries — the only subproject with an external dependency). 10 test suites pass, including a byte-for-byte golden-output comparison. Ships as a single self-contained binary with a CI-friendly non-zero exit code on findings.

**c-linter** — Five rules (`CL001`–`CL005`) covering snake_case naming, line length, trailing whitespace, magic numbers in comparisons, and K&R/Allman brace-style consistency, built on a small hand-written lexer that's deliberately never shared with `c-compiler-llvm`, keeping the subproject independent per this repo's convention. The lexer is tolerant by design — unterminated comments/literals and unmodeled punctuation never cause an error, since a linter has to process real-world C it doesn't fully model. All 7 test suites pass. Reporting only, with a CI-friendly non-zero exit code on findings.

**build-tool** — Parses a Makefile into rules, resolves them into a dependency graph with cycle detection and memoization (a shared prerequisite builds exactly once), skips up-to-date targets via mtime staleness, and runs outstanding recipes serially in topological order with `-k`/`--keep-going` support. `-j` parallelism is a deliberate non-goal: it only affects wall-clock build speed, not correctness, so this small tool trades it for a much simpler single-threaded executor. 22 tests pass, and it's the only subproject with CI wired up so far (path-scoped GitHub Actions workflow, gated on a CMake build and `ctest`).

**c-linker** — Parses real ELF64 x86-64 relocatable object files (the same output `clang -c` produces) via pointer-casting onto a small vendored subset of the ELF64 structures, rather than inventing a toy format. Merges `.text`/`.data` across input files, resolves symbols with undefined-symbol and multiple-definition detection, patches `Abs64`/`Pc32`/`Plt32` relocation sites using the real x86-64 psABI formulas, and emits a real, minimal, loadable static ELF executable — runnable directly on x86-64 Linux, verified independently with `readelf`. All 6 test suites pass, every one exercising real `.o` files compiled on the fly by `clang` rather than checked-in binary fixtures.

## Getting Started

Each project builds independently — see its README for details.

```bash
# c-preprocessor (C++20/CMake, no external dependencies)
cd c-preprocessor && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure

# MiniC compiler (C++20/CMake, requires LLVM 17+)
cd c-compiler-llvm && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure

# C static analyzer (C++20/CMake, fetches tree-sitter + tree-sitter-c via FetchContent)
cd c-static-analyzer && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure

# c-linter (C++20/CMake, no external dependencies)
cd c-linter && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure

# build-tool (C++20/CMake, no external dependencies)
cd build-tool && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure

# c-linker (C++20/CMake, clang required on PATH to build test/example fixtures)
cd c-linker && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure
```

## References

Each subproject cites the sources specific to its own stage of the pipeline
in its own README — see
[c-preprocessor](c-preprocessor/README.md#references),
[c-compiler-llvm](c-compiler-llvm/README.md#references),
[c-static-analyzer](c-static-analyzer/README.md#references),
[c-linter](c-linter/README.md#references), and
[c-linker](c-linker/README.md#references). At the
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
- Levine, John R. *Linkers and Loaders*. Morgan Kaufmann, 2000. — the
  primary reference for `c-linker`, the final stage of this toolchain's
  pipeline: symbol resolution, section merging, and relocation processing.

## License

Each subproject is MIT licensed; see the `LICENSE` file in this directory and in each subproject.
