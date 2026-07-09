# c-static-analyzer

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake 3.20+](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> A lightweight static analyzer for C code that catches common quality, correctness, and maintainability issues before runtime — built from scratch in C++20 as a sibling to a [MiniC compiler](../c-compiler) and a [build tool](../build-tool). It only parses (via [tree-sitter](https://tree-sitter.github.io/tree-sitter/)) — it never compiles or executes the code it scans.

It parses `.c`/`.h` files, walks the resulting syntax tree with six independent rules, and reports file-and-line diagnostics with stable rule IDs (`SA001`–`SA006`), exiting non-zero on findings — suitable for local use or CI.

---

## Table of Contents

- [Checks](#checks)
- [Repo Structure](#repo-structure)
- [Quick Start](#quick-start)
- [Usage](#usage)
- [Configuration](#configuration)
- [Architecture](#architecture)
- [Testing](#testing)
- [Build & Run](#build--run)
- [License](#license)
- [References](#references)

---

## Checks

| Rule | Severity | Description |
|------|----------|-------------|
| `SA001` | Warning | Function cyclomatic complexity exceeds threshold |
| `SA002` | Warning | Local variable is assigned but never used |
| `SA003` | Warning | Control flow nesting depth exceeds threshold |
| `SA004` | Error   | Non-void function may not return a value on all paths |
| `SA005` | Warning | Unreachable code after `return`, `break`, `continue`, or `goto` |
| `SA006` | Warning | Local variable may be used before being initialized |

## Repo Structure

```text
c-static-analyzer/
├── CMakeLists.txt
├── scripts/
│   └── configure.sh              # cmake -S . -B build -G Ninja
├── include/
│   ├── cli.h                     # ScanArgs, parseArgs(), buildConfig(), run()
│   ├── config.h                  # Config, loadConfig() — TOML-subset parser
│   ├── analyzer.h                # file discovery, isExcluded(), analyzeFile()/analyzePaths()
│   ├── diagnostic.h              # Diagnostic struct, ordering, toString()
│   ├── fnmatch.h                 # glob matching for exclude patterns
│   ├── visitor.h                 # shared tree-sitter C-API traversal helpers
│   ├── snippet.h                 # --show-source caret/snippet rendering
│   └── rules/
│       ├── rule.h                # Rule interface
│       └── sa00N_*.h             # one header per rule
├── src/                          # mirrors include/, plus main.cpp
│   └── rules/sa00N_*.cpp
├── tests/                        # one executable per suite (see Testing above)
├── examples/
│   └── sample_issues.c           # triggers every rule; the golden fixture
└── docs/
    └── SPEC.md                   # exact rule semantics (complexity scoring,
                                   # nesting rules, exit-guarantee logic, etc.)
```

## Quick Start

```bash
./scripts/configure.sh
cmake --build build
./build/c-static-analyzer scan path/to/project
```

### Example

Given this C snippet:

```c
const char *classify(int x) {
    if (x > 0) {
        return "positive";
    } else if (x < 0) {
        return "negative";
    }
}
```

The analyzer reports:

```
example.c:1: SA004 Function `classify` may not return a value on all code paths
```

`SA006` catches the mirror-image mistake — reading a local before it's ever
written:

```c
int compute(int flag) {
    int result;
    return result;
}
```

```
$ c-static-analyzer scan example.c --no-config --select SA006
example.c:3: SA006 Local variable `result` may be used before being initialized

1 issue(s) found.
```

Pass `--show-source` to additionally print the offending source line and a
caret under any diagnostic — purely additive, the default single-line
output above is unchanged:

```
$ c-static-analyzer scan example.c --no-config --select SA006 --show-source
example.c:3:12: SA006 Local variable `result` may be used before being initialized
    return result;
           ^

1 issue(s) found.
```

## Usage

```
c-static-analyzer scan [paths...] [--max-complexity N] [--max-nesting N]
                       [--select SA001,SA002] [--exclude PATTERN]... [--no-config]
                       [--show-source]
```

| Flag | Default | Description |
|------|---------|-------------|
| `--max-complexity N` | `10` | Cyclomatic complexity threshold |
| `--max-nesting N` | `4` | Control flow nesting depth threshold |
| `--show-source` | off | Also print the source line and a caret under each diagnostic |
| `--select SA001,SA002` | all | Run only the specified rule IDs |
| `--exclude PATTERN` | — | Glob pattern to exclude (repeatable) |
| `--no-config` | — | Ignore `.c-static-analyzer.toml` |

**Exit codes**

| Code | Meaning |
|------|---------|
| `0` | No issues found |
| `1` | One or more diagnostics reported |
| `2` | Usage error (e.g. path does not exist) |

**Sample output**

```
src/app.c:12: SA001 Function `parse_request` has cyclomatic complexity 14 (threshold 10)
src/app.c:34: SA002 Local variable `unused` is assigned but never used
src/util.c:48: SA004 Function `convert` may not return a value on all code paths
```

By default the scanner skips common non-project directories: `.git`, `build`, `dist`, `cmake-build-debug`, `cmake-build-release`, `CMakeFiles`, `out`, `vendor`, `third_party`.

## Configuration

Create a `.c-static-analyzer.toml` file in (or above) the directory you're scanning — the nearest one wins. CLI flags take precedence over file settings.

```toml
exclude        = ["tests/fixtures/*"]
max_complexity = 10
max_nesting    = 4
enabled_rules  = ["SA001", "SA002", "SA004"]
```

> `enabled_rules = []` (the default) means all rules are enabled. This is a small hand-rolled parser covering exactly this flat schema (string/integer/string-array assignments, `#` comments) — not a general-purpose TOML implementation.

## Architecture

```
Makefile/C source text
     │
     ▼
  cli.{h,cpp} ── parses "scan" flags, loads/merges config.{h,cpp}
     │
     ▼
  analyzer.{h,cpp} ── discovers .c/.h files, applies exclude patterns
     │
     ▼
  tree-sitter (fetched) ── parses each file into a concrete syntax tree
     │
     ▼
  visitor.{h,cpp} ── shared CST traversal helpers (walk, loc, functionName)
     │
     ▼
  rules/sa00N_*.{h,cpp} ── six independent rules, each returning
  │                        Diagnostic values
     ▼
  diagnostic.{h,cpp} ── sorted output (path, line, col, ruleId, message)
                        + exit code
```

Unlike the original Rust implementation (which used tree-sitter's Rust bindings), this port links directly against tree-sitter's **C API** (`tree_sitter/api.h`) and the `tree-sitter-c` grammar as plain C static libraries, fetched via CMake `FetchContent` — see [CMakeLists.txt](CMakeLists.txt) for why the grammar's own bundled `CMakeLists.txt` (which wires up code regeneration through the `tree-sitter` CLI) is deliberately bypassed in favor of compiling its pre-generated `parser.c` directly.

**Adding a new rule** — create `include/rules/saXXX_name.h` + `src/rules/saXXX_name.cpp` implementing the `Rule` interface, then register it in `kAllRules` in `src/analyzer.cpp`. No other files need to change.

## Testing

12 test suites, run with `ctest`.

| Suite | What it checks |
|---|---|
| `fnmatch_test` | `*`/`?`/`[set]`/`[!set]` glob matching, case-sensitivity |
| `config_test` | Default rule enablement, `enabled_rules` restriction, TOML loading (full + partial + missing file) |
| `analyzer_test` | Default-excluded directories, custom exclude patterns, `.c`/`.h` discovery, `isExcluded()` semantics |
| `sa001_complexity_test` | Cyclomatic complexity scoring and independence across functions |
| `sa002_unused_variables_test` | Unused-local detection, underscore-prefix opt-out, global mutation, array-size/initializer use |
| `sa003_nesting_test` | Nesting depth, `elif`-chain vs. real-`else` distinction, single report per function, `switch`/`case` nesting |
| `sa004_missing_return_test` | Missing-`else` detection, exhaustive if/else(-if), void functions, infinite loops with/without `break` |
| `sa005_unreachable_code_test` | Code after `return`/`break` (including inside loops and `case` bodies) |
| `sa006_uninitialized_variable_test` | Read-before-write detection, initializer/array/underscore exemptions, field-by-field struct writes |
| `snippet_test` | Caret column alignment (0-indexed), `formatWithSource`'s 1-indexed display column + snippet output |
| `golden_test` | Byte-for-byte comparison against [examples/sample_issues.c](examples/sample_issues.c)'s frozen expected output |
| `cli_test` | Subprocess exercise of the real binary: clean exit 0, findings exit 1, `--select` filtering, `--show-source`, missing path exit 2 |

```bash
$ ctest --test-dir build --output-on-failure
Test project c-static-analyzer/build
      Start  1: fnmatch_test
 1/12 Test  #1: fnmatch_test .....................   Passed
      ...
12/12 Test #12: cli_test .........................   Passed

100% tests passed, 0 tests failed out of 12
```

Running the analyzer on [examples/sample_issues.c](examples/sample_issues.c) (a file written to trigger every rule) confirms end-to-end behavior:

```
$ c-static-analyzer scan examples/sample_issues.c --no-config
examples/sample_issues.c:3: SA001 Function `complex_calc` has cyclomatic complexity 12 (threshold 10)
examples/sample_issues.c:18: SA004 Function `classify` may not return a value on all code paths
examples/sample_issues.c:31: SA003 Control flow nested 5 levels deep (threshold 4)
examples/sample_issues.c:41: SA002 Local variable `unused` is assigned but never used
examples/sample_issues.c:45: SA005 Unreachable code after `return`

5 issue(s) found.
```

## Build & Run

Dependencies: CMake 3.20+, a C++20 compiler, Ninja (or another CMake generator), and network access at configure time (to fetch tree-sitter + the C grammar via `FetchContent`).

```bash
./scripts/configure.sh
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

[MIT](LICENSE)

## References

- [tree-sitter](https://tree-sitter.github.io/tree-sitter/) — incremental parsing library used for syntax tree construction
- [tree-sitter-c](https://github.com/tree-sitter/tree-sitter-c) — C grammar for tree-sitter
- [Cyclomatic Complexity (McCabe, 1976)](https://doi.org/10.1109/TSE.1976.233837) — metric used by SA001
- [MISRA C](https://www.misra.org.uk/) — industry coding standard for C; a reference point for rule design
- [Clang Static Analyzer](https://clang-analyzer.llvm.org/) — production-grade C/C++ analyzer; useful comparison point
- [cppcheck](https://cppcheck.sourceforge.io/) — open-source C/C++ static analysis tool
