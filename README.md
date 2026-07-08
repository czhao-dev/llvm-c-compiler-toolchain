# c-llvm-toolchain

[![build-tool CI](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml/badge.svg)](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A small C toolchain built from scratch, one piece at a time: a preprocessor, a compiler, a static analyzer, a style linter, a build tool, and a linker.

Each subproject is independent and self-contained — its own language, build system, tests, and README — but together they cover the path from source code to a finished build: check the code, compile it, build it.

---

## Projects

| Project | Language | Description |
|---|---|---|
| [c-preprocessor](c-preprocessor/README.md) | C++20 | A minimal C preprocessor: `#include` file inclusion, object-like `#define`/`#undef` macros with hide-set-safe recursive expansion, and `//`/`/* */` comment stripping. Function-like macros, conditional compilation, and `##`/`#` are explicit non-goals — each is a hard error rather than a silent no-op. |
| [c-compiler-llvm](c-compiler-llvm/README.md) | C++20 / LLVM | **MiniC** — a compiler for a statically-typed subset of C. Hand-written lexer, recursive-descent parser, semantic analyzer, and LLVM IR codegen producing native binaries, cross-validated against clang. |
| [c-static-analyzer](c-static-analyzer/README.md) | C++20 | A lightweight static analyzer for C code. Parses `.c`/`.h` files with tree-sitter (no compilation needed) and reports diagnostics for complexity, unused variables, nesting depth, missing returns, and unreachable code. |
| [c-linter](c-linter/README.md) | C++20 | A style/formatting linter for C: snake_case naming, line length (80 cols) and trailing whitespace, magic-number detection in comparisons, and K&R/Allman brace-style consistency. Reporting only — no auto-fixing, and no semantic checks (that's c-static-analyzer's job). |
| [build-tool](build-tool/README.md) | C++20 | A dependency-graph-aware build tool implementing core GNU Make semantics. Resolves a Makefile into a topologically-ordered plan, checks mtime-based staleness, and executes recipes serially with cycle detection and `-k`/`--keep-going` support. |
| [c-linker](c-linker/README.md) | C++20 | A static linker for real ELF64 x86-64 object files: merges `.text`/`.data` sections across multiple `.o` files, resolves symbols (undefined-symbol and multiple-definition detection), applies `Abs64`/`Pc32` relocation fixups, and writes a real, runnable static ELF executable. No dynamic linking, archive parsing, or LTO. |

## Highlights

**c-preprocessor** — A four-stage pipeline (comment stripper → directive/include line-driver → tokenizer → hide-set-based macro rescanner) built on `libpp_core`, with zero external dependencies. `#include` resolves double-quoted paths relative to the *including* file's directory, then against `-I` search directories in order; circular includes are detected and reported with the full chain, while diamond includes are intentionally left un-deduplicated, since there are no include guards. Recursive macro expansion (a macro's replacement can reference another macro, e.g. `TWO_PI` → `PI * 2` → `3 * 2`) terminates correctly on self-referential and mutually recursive definitions via the standard "blue paint" hide-set algorithm — every token produced by expanding macro `M` carries `M` in its hide set, so an identifier already in its own hide set is emitted literally instead of looping forever, matching real `cpp` behavior on inputs like `#define X X + 1` rather than erroring out. Comment stripping replaces a multi-line block comment with the same number of newlines it contained, so line numbers in diagnostics stay accurate straight through it. Function-like macros, conditional compilation (`#ifdef`/`#if`), and `##`/`#` are explicit non-goals — each is a hard `file:line` compile error rather than a silent no-op. All 7 test suites pass, including a byte-for-byte golden-output comparison against a multi-file example and a subprocess-level exercise of the CLI.

**MiniC compiler** — A complete four-stage pipeline (lexer → recursive-descent parser → semantic analyzer → LLVM IR generator) compiled into `libminic_core.a` behind a thin CLI, with `--emit-tokens`/`--emit-ast`/`--emit-ir` flags exposing every stage's output for inspection. The language subset covers `int`/`float`/`char`/`void`, pointers (including address-of/dereference and null-pointer comparisons), fixed-size arrays with pointer decay, named structs/unions/enums with first-class whole-value assignment, the full arithmetic/comparison/logical/bitwise/ternary/compound-assignment operator set, and `if`/`while`/`for`/`do`-`while`/`switch`-with-real-fallthrough/`goto`/labels — recursive functions work because the IR generator resolves forward references correctly. The semantic analyzer catches undeclared identifiers, type mismatches, wrong argument counts, and return-type mismatches with precise `file:line:col` diagnostics and source-snippet carets. All five test suites pass, and all nine example programs (fibonacci, fizzbuzz, gcd, pointer swap, array sum, struct point, bit ops, control flow, sum of squares) produce byte-for-byte identical output to `clang` compiling the same source. A targeted correctness review found and fixed four real bugs — misordered assignment diagnostics, a temp-file leak on write failure, missing float-exponent lexing (`1e5`), and a sema/codegen type disagreement on unary negation of `char` — each verified with a regression case. `-O1`/`-O2`/`-O3` run LLVM's real new-pass-manager pipeline (`mem2reg`, `instcombine`, `simplifycfg`, `tailcallelim`, loop unrolling); `-O2` gets `fibonacci(40)` to a measured 1.4× speedup over `-O0` and converts its tail recursion into a loop.

**C static analyzer** — Five rules (`SA001`–`SA005`) covering cyclomatic complexity, unused variables, control-flow nesting depth, non-exhaustive return paths, and unreachable code after `return`/`break`/`continue`/`goto`, built directly against tree-sitter's C API and the `tree-sitter-c` grammar (fetched via CMake `FetchContent` and compiled as plain C static libraries, deliberately bypassing the grammar's own bundled build in favor of compiling its pre-generated `parser.c` directly — the only subproject with an external dependency). File discovery skips common non-project directories (`.git`, `build`, `dist`, `vendor`, `third_party`, ...) by default, and behavior is configurable via CLI flags or a discovered `.c-static-analyzer.toml` (rule selection, complexity/nesting thresholds, exclude globs), with CLI flags always taking precedence over the file. Adding a new rule is a two-file change (a header implementing the `Rule` interface plus one registration line), by design. 10 test suites pass, including a byte-for-byte golden-output comparison against a fixture engineered to trigger every rule at once, plus a subprocess CLI test asserting the exit-code contract (`0` clean, `1` findings, `2` usage error).

**c-linter** — Five rules (`CL001`–`CL005`) covering snake_case naming, line length (80 cols default), trailing whitespace, magic numbers in comparisons, and K&R/Allman brace-style consistency, built on a small hand-written lexer that's deliberately never shared with `c-compiler-llvm`, keeping the subproject independent per this repo's convention. The lexer is tolerant by design — unterminated comments/literals and unmodeled punctuation fall through to a generic token type rather than erroring, since a linter has to process real-world, possibly-broken C it doesn't fully model. Naming (CL001) is a pure token-level check with no symbol table, so every *occurrence* of a badly-named identifier is flagged, not just its declaration; magic-number detection (CL004) exempts `0`, `1`, and `-1` as common sentinel values; brace-style checking (CL005) matches the closing parenthesis of an `if`/`while` condition through nested parens against the placement of the following brace. Line length and trailing whitespace run as a raw-text pass before tokenization even happens. All 7 test suites pass. Reporting only — no auto-fixing, no indentation tracking, and no semantic checks (that boundary belongs to `c-static-analyzer`) — with CI-friendly exit codes (`0`/`1`/`2`).

**build-tool** — Parses a Makefile into rules (explicit prerequisites, tab-indented recipes, `.PHONY` declarations before or after a rule, inline comments), resolves them into a dependency graph via a single recursive depth-first walk that both detects cycles and produces a valid topological order as a side effect (a node's prerequisites are always resolved before the node itself, so no separate scheduling pass is needed), skips up-to-date targets via mtime staleness, and runs outstanding recipes serially with fail-fast or `-k`/`--keep-going` semantics. Memoization guarantees a diamond-shaped shared prerequisite builds exactly once. `-j` parallelism is a deliberate non-goal: it only affects wall-clock build speed, not correctness, so this small tool trades it for a much simpler single-threaded executor with no thread pool or work-stealing queue. Variable expansion and pattern rules are likewise out of scope — this implements the core Make mental model (targets, prerequisites, recipes, staleness) precisely rather than a large surface approximately. 22 tests pass across three suites (Makefile parsing, dependency planning, and full binary end-to-end invocations covering a clean re-run and a touch-triggered rebuild), and it's the only subproject with CI wired up so far (path-scoped GitHub Actions workflow, gated on a CMake build and `ctest`).

**c-linker** — Parses real ELF64 x86-64 relocatable object files (`ET_REL`, the same output `clang -c` produces) via pointer-casting onto a small vendored subset of the ELF64 structures (macOS ships no `<elf.h>`, so this linker defines the handful of fields it actually reads/writes), rather than inventing a toy format. The pipeline reads each object file, builds a global symbol table while flagging multiple-definition conflicts, checks every relocation's referenced symbol actually resolves somewhere, merges `.text` and `.data` sections across all input files in order with per-file alignment padding, patches `Abs64` (`write64(S+A)`) and `Pc32`/`Plt32` (`write32(S+A-P)`) relocation sites in place using the real x86-64 psABI formulas, and emits a real, minimal, loadable static ELF executable — two page-aligned `PT_LOAD` segments, no section headers required to run — directly executable on x86-64 Linux and verified independently with `readelf`. No dynamic linking (no PLT/GOT), archive (`.a`) parsing, or link-time optimization. All 6 test suites pass, every one exercising real `.o` files compiled on the fly by `clang --target=x86_64-unknown-linux-gnu` against freestanding fixtures rather than checked-in binary fixtures, including a relocation test that hand-verifies patch math against real compiled relocations and a fabricated out-of-range case that produces a clean `RelocationOverflow` error instead of corrupting bytes.

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
