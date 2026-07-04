# build-tool

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake 3.20+](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](../LICENSE)
[![CI](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml/badge.svg)](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml)

> A dependency-graph-aware build tool implementing core GNU Make semantics — built from scratch in C++20 as the third piece of a C toolchain alongside a [MiniC compiler](../c-compiler-llvm) and a [C static analyzer](../c-static-analyzer). Variable expansion, pattern rules, and parallel execution are explicit non-goals for this small implementation; see [Supported Features](#supported-features) for the exact boundary.

It parses a Makefile into target/prerequisite rules, resolves them into a dependency graph (with cycle detection and memoization so a shared dependency is built exactly once), checks mtime-based staleness to skip up-to-date targets, and runs outstanding recipes serially in topological order.

---

## Table of Contents

- [Overview](#overview)
- [Supported Features](#supported-features)
- [Example](#example)
- [Architecture](#architecture)
- [Testing](#testing)
- [Repo Structure](#repo-structure)
- [Build & Run](#build--run)
- [License](#license)
- [References](#references)

---

## Overview

`build-tool` reads a Makefile, builds a dependency graph out of its rules, and runs whatever recipes are actually needed to bring the requested targets up to date — in the order their dependencies require, never running a target's recipe before its prerequisites have finished.

It deliberately does **not** try to be a drop-in `make` replacement. There's no variable expansion, no pattern rules, and no parallel execution — just the core mental model of Make (targets, prerequisites, recipes, staleness) implemented precisely and tested thoroughly, rather than a large surface implemented approximately.

## Supported Features

**Supported (v1 "Core" scope)**

- Explicit rules with prerequisites and tab-indented recipes
- `.PHONY` target declarations (rebuild always, even if the file is newer than its prereqs)
- mtime-based staleness: skip targets whose output is already newer than all inputs
- Default goal (first non-dot target in the Makefile)
- Fail-fast on a recipe error (any non-zero exit code)
- `-k` / `--keep-going`: continue building independent branches after a failure

**Not yet supported** (see [docs/SPEC.md](docs/SPEC.md) for the full list)

- Variable expansion (`$(VAR)`, automatic variables `$@`/`$<`/`$^`)
- Pattern rules (`%.o: %.c`)
- `-n` dry-run mode, `-f` Makefile path override
- Built-in Make functions (`$(wildcard ...)`, `$(shell ...)`)
- Parallel execution (`-j`) — recipes run serially, one at a time, in topological order. `-j` only affects wall-clock build speed, not correctness, so for a small tool like this one, a single-threaded executor is a deliberate simplification rather than a missing feature.

## Example

```makefile
.PHONY: all clean

all: app

app: main.o utils.o
	cc -o app main.o utils.o

main.o: main.c common.h
	cc -c main.c

utils.o: utils.c common.h
	cc -c utils.c

clean:
	rm -f app main.o utils.o
```

```
$ build-tool
cc -c main.c
cc -c utils.c
cc -o app main.o utils.o
$ ./app
2 + 3 = 5
$ build-tool
$ # (nothing rebuilt — everything already up to date)
$ build-tool clean
rm -f app main.o utils.o
```

This exact example lives in [`examples/`](examples).

## Architecture

```
Makefile text
     │
     ▼
  makefile.{h,cpp} ── parses rules, .PHONY declarations, comments
     │
     ▼
  planner.{h,cpp} ── resolves rules into a topologically-ordered Plan,
  │                   detects cycles, handles missing targets, checks
  │                   mtime staleness, and executes recipes serially
  │                   with fail-fast / keep-going semantics
     │
     ▼
  build-tool binary ── recipe lines run via sh -c, exit codes propagated
```

Unlike a build system meant to scale to large codebases, `build-tool` resolves prerequisites via a single recursive depth-first walk that both detects cycles and produces a valid topological order as a side effect (a node's prerequisites are always resolved — and thus appended to the plan — before the node itself), so executing the plan is just a single pass over that list on one thread. No thread pool, work-stealing queue, or task scheduler is needed.

## Testing

22 tests across three suites, run with `ctest`.

| Suite | What it checks |
|---|---|
| `makefile_parser_test` (11 cases) | Rule parsing, `.PHONY` declarations (before and after the rule), inline comment stripping, default goal resolution, error cases (orphan recipe line, header without colon), and `discoverMakefile()` finding or not finding a Makefile on disk. |
| `planner_test` (8 cases) | Diamond dependency built exactly once, cycle detection, missing-target error, existing file treated as a leaf, mtime staleness (up-to-date target skipped), phony target rebuilt even when newer, empty-recipe phony target, failed prerequisite causes a dependent to be skipped (under both `keepGoing` values). |
| `build_tool_e2e_test` (3 cases) | Full binary invocations: a diamond build runs recipes once, then skips them on a clean re-run, then rebuilds both after touching the shared prerequisite; a failing recipe returns exit code 1; a missing Makefile returns exit code 2. |

```bash
$ ctest --test-dir build --output-on-failure
Test project build-tool/build
    Start 1: makefile_parser_test
1/3 Test #1: makefile_parser_test .............   Passed
    Start 2: planner_test
2/3 Test #2: planner_test .....................   Passed
    Start 3: build_tool_e2e_test
3/3 Test #3: build_tool_e2e_test ..............   Passed

100% tests passed, 0 tests failed out of 3
```

## Repo Structure

```text
build-tool/
├── CMakeLists.txt
├── scripts/
│   └── configure.sh          # cmake -S . -B build -G Ninja
├── include/
│   ├── cli.h                 # Args, parseArgs() — -k/--keep-going, target list
│   ├── makefile.h             # Rule, ParsedMakefile, parseMakefile(), discoverMakefile()
│   └── planner.h              # BuildStatus, Plan, plan(), execute()
├── src/
│   ├── cli.cpp
│   ├── makefile.cpp
│   ├── planner.cpp
│   └── main.cpp               # binary entry point
├── tests/
│   ├── makefile_parser_test.cpp  # 11 cases: parsing, .PHONY, discover
│   ├── planner_test.cpp          # 8 cases: diamond, cycle detection, staleness
│   └── build_tool_e2e_test.cpp   # 3 cases: end-to-end binary invocations
├── examples/                  # a small compiled C program built by build-tool itself
└── docs/
    └── SPEC.md                # the exact Makefile subset this tool implements
```

## Build & Run

Dependencies: CMake 3.20+, a C++20 compiler, Ninja (or another CMake generator).

```bash
./scripts/configure.sh
cmake --build build
ctest --test-dir build --output-on-failure
```

### CLI usage

```
build-tool [-k] [target ...]
```

| Flag | Description |
|------|-------------|
| `-k`, `--keep-going` | Continue building independent branches after a failure |
| `target` | One or more targets to build (default: the Makefile's default goal) |

**Exit codes**

| Code | Meaning |
|------|---------|
| `0` | All requested targets are up to date or were built successfully |
| `1` | At least one recipe failed |
| `2` | Usage error or missing Makefile |

## License

[MIT](../LICENSE)

## References

- Feldman, Stuart I. "Make — A Program for Maintaining Computer Programs." *Software: Practice and Experience* 9, no. 4 (1979): 255–265. — the original paper describing Make's dependency-graph-plus-recipes model that this tool implements a small, precise subset of.
- GNU Make Manual, chapters 2–4 (rules, recipes, and phony targets) — the normative reference for the specific Makefile syntax and staleness semantics this subset is modeled on.
- Levine, John R. *Linkers and Loaders*. Morgan Kaufmann, 2000. — companion reading for what happens just past this tool's own scope, once `build-tool` hands compiled objects off to the linker.
