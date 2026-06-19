# build-tool

[![CI](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml/badge.svg)](https://github.com/czhao-dev/c-llvm-toolchain/actions/workflows/build-tool.yml)

A parallel, dependency-graph-aware build tool that follows standard make semantics — built from scratch in Rust as the third piece of a C toolchain alongside a [MiniC compiler](../c-compiler-llvm) and a [C static analyzer](../c-static-analyzer).

It parses a Makefile into target/prerequisite rules, resolves them into a DAG (with cycle detection and memoization so a shared dependency is built exactly once), checks mtime-based staleness to skip up-to-date targets, and executes outstanding recipes in parallel through a vendored work-stealing thread pool.

---

## Table of Contents

- [Architecture](#architecture)
- [Supported Features](#supported-features)
- [Quick Start](#quick-start)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [License](#license)

---

## Architecture

```
Makefile text
     │
     ▼
  makefile.rs ── parses rules, .PHONY declarations, comments
     │
     ▼
  planner.rs ── resolves rules → TaskGraph, checks mtime staleness,
  │              detects cycles, handles missing targets, tracks
  │              keep-going/fail-fast mode
     │
     ▼
  engine/ ── vendored work-stealing Runtime + DAG scheduler
  ├─ runtime.rs / worker.rs / steal.rs ── multi-priority work-stealing pool
  ├─ dependency.rs ── TaskGraph / run_graph (DAG execution over mpsc)
  └─ handle.rs / cancellation.rs ── panic-safe JoinHandle, cancellation tokens
     │
     ▼
  build-tool binary ── shell recipes run via sh -c, exit codes propagated
```

The engine layer is a full from-scratch work-stealing thread pool (three priority levels: High/Normal/Background) vendored from the companion project [work-stealing-thread-pool](https://github.com/czhao-dev/work-stealing-thread-pool). It is copied directly into `src/engine/` so `build-tool` is a fully self-contained crate with no git dependencies.

---

## Supported Features

**Supported (v1 "Core" scope)**

- Explicit rules with prerequisites and tab-indented recipes
- `.PHONY` target declarations (rebuild always, even if file is newer than prereqs)
- mtime-based staleness: skip targets whose output is already newer than all inputs
- Default goal (first non-dot target in the Makefile)
- Parallel builds via `-j N` (default 1, like real Make)
- Fail-fast on a recipe error (any non-zero exit code)
- `-k` / `--keep-going`: continue building independent branches after a failure

**Not yet supported**

- Variable expansion (`$(VAR)`, automatic variables `$@`/`$<`/`$^`)
- Pattern rules (`%.o: %.c`)
- `-n` dry-run mode
- `-f` Makefile path override
- Built-in Make functions (`$(wildcard ...)`, `$(shell ...)`)

---

## Quick Start

```bash
# Build the release binary
cargo build --release

# Run in the directory containing your Makefile
./target/release/build-tool -j4
```

Example `Makefile`:

```makefile
.PHONY: all

all: app

app: main.o utils.o
	cc -o app main.o utils.o

main.o: main.c common.h
	cc -c main.c

utils.o: utils.c common.h
	cc -c utils.c
```

---

## Usage

```
build-tool [-j N] [-k] [target ...]
```

| Flag | Default | Description |
|------|---------|-------------|
| `-j N` | `1` | Number of parallel worker threads |
| `-k`, `--keep-going` | off | Continue building independent branches after a failure |
| `target` | first non-dot goal | One or more targets to build |

**Exit codes**

| Code | Meaning |
|------|---------|
| `0` | All requested targets are up to date or were built successfully |
| `1` | At least one recipe failed |
| `2` | Usage error or missing Makefile |

---

## Project Structure

```text
build-tool/
├── Cargo.toml
├── src/
│   ├── lib.rs                     # crate root; re-exports run_graph, Runtime
│   ├── cli.rs                     # argument parsing (-j, -k, target list)
│   ├── makefile.rs                # Makefile lexer/parser, .PHONY, discover()
│   ├── planner.rs                 # rules → TaskGraph, mtime staleness, fail-fast/-k
│   ├── bin/
│   │   └── build_tool.rs         # binary entry point
│   └── engine/                   # vendored work-stealing engine + DAG scheduler
│       ├── mod.rs                # re-exports all public engine types
│       ├── runtime.rs            # Runtime API, shutdown, metrics wiring
│       ├── worker.rs             # worker main loop, idle doorbell
│       ├── steal.rs              # per-priority work-stealing deques
│       ├── task.rs               # type-erased Job alias
│       ├── handle.rs             # JoinHandle, panic-safe results
│       ├── priority.rs           # Priority enum and scan order
│       ├── cancellation.rs       # CancellationToken / CancellationContext
│       ├── dependency.rs         # TaskGraph / run_graph over mpsc channels
│       └── metrics.rs            # runtime counters
└── tests/
    ├── makefile_parser.rs        # 11 tests: parsing, .PHONY, discover
    ├── planner.rs                # 8 tests: diamond, cycle detection, staleness
    └── build_tool_e2e.rs         # 3 tests: end-to-end binary invocations
```

---

## Testing

22 integration tests across three files, run with `cargo test`.

- **`makefile_parser.rs`** (11 tests) — rule parsing, `.PHONY` declarations (before and after the rule), inline comment stripping, default goal resolution, error cases (orphan recipe line, header without colon), and `discover()` finding or not finding a Makefile on disk.
- **`planner.rs`** (8 tests) — diamond dependency built exactly once, cycle detection, missing target error, existing file treated as a leaf, mtime staleness (up-to-date target skipped), phony target rebuilt even when newer, empty-recipe phony target, failed prerequisite causes dependent to be skipped.
- **`build_tool_e2e.rs`** (3 tests) — full binary invocations: diamond build runs recipes once then skips them on a clean re-run and rebuilds both after a `touch`; a failing recipe returns exit code 1; a missing Makefile returns exit code 2.

```bash
cargo test                         # run all 22 tests
cargo clippy --all-targets -- -D warnings
cargo fmt -- --check
```

---

## License

[MIT](../LICENSE)
