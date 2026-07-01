# c-llvm-toolchain

[![parallel-make CI](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/parallel-make.yml/badge.svg)](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/parallel-make.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A small C toolchain built from scratch, one piece at a time: a compiler, a static analyzer, and a parallel build tool.

Each subproject is independent and self-contained — its own language, build system, tests, and README — but together they cover the path from source code to a finished build: check the code, compile it, build it.

---

## Projects

| Project | Language | Description |
|---|---|---|
| [c-compiler-llvm](c-compiler-llvm) | C++17 / LLVM | **MiniC** — a compiler for a statically-typed subset of C. Hand-written lexer, recursive-descent parser, semantic analyzer, and LLVM IR codegen producing native binaries, cross-validated against clang. |
| [c-static-analyzer](c-static-analyzer) | Rust | A lightweight static analyzer for C code. Parses `.c`/`.h` files with tree-sitter (no compilation needed) and reports diagnostics for complexity, unused variables, nesting depth, missing returns, and unreachable code. |
| [parallel-make](parallel-make) | Rust | A parallel, dependency-graph-aware build tool that follows GNU Make's documented semantics. Resolves a Makefile into a DAG, checks mtime-based staleness, and executes recipes in parallel via a vendored work-stealing thread pool. |

## Highlights

**MiniC compiler** — A complete four-stage pipeline (lexer → recursive-descent parser → semantic analyzer → LLVM IR generator) compiled into `libminic_core.a` behind a thin CLI. All five test suites pass, and all nine example programs produce byte-for-byte identical output to `clang` on the same source. `-O1`/`-O2`/`-O3` run LLVM's real optimization pipeline (`mem2reg`, `instcombine`, `tailcallelim`, loop unrolling); `-O2` gets `fibonacci(40)` to a measured 1.4× speedup over `-O0`.

**C static analyzer** — Five rules (`SA001`–`SA005`) covering cyclomatic complexity, unused variables, nesting depth, missing returns, and unreachable code, built on tree-sitter so no compilation step is required. 44 tests pass — 36 unit tests plus 8 integration tests including a byte-for-byte golden-output comparison. Ships as a single self-contained binary with a CI-friendly non-zero exit code on findings.

**parallel-make** — Parses a Makefile into rules, resolves them into a dependency DAG with cycle detection and memoization, skips up-to-date targets via mtime staleness, and runs outstanding recipes through a three-priority-level work-stealing thread pool vendored from a companion project. Supports `-j N` parallelism and `-k`/`--keep-going`. 22 tests pass, and it's the only subproject with CI wired up so far (path-scoped GitHub Actions workflow, gated on `cargo fmt`, `clippy`, and `cargo test`).

## Getting Started

Each project builds independently — see its README for details.

```bash
# MiniC compiler (C++17/CMake, requires LLVM 17+)
cd c-compiler-llvm && ./scripts/configure.sh && cmake --build build
ctest --test-dir build --output-on-failure

# C static analyzer (Rust)
cd c-static-analyzer && cargo build --release && cargo test

# parallel-make (Rust)
cd parallel-make && cargo build --release && cargo test
```

## License

Each subproject is MIT licensed; see the `LICENSE` file in this directory and in each subproject.
