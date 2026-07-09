# c-preprocessor

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

> A minimal C preprocessor: file inclusion, object-like macros, and comment
> stripping — the small, well-defined slice of `cpp` that a companion tool
> like `c-compiler` deliberately leaves out.

---

## Table of Contents

- [Overview](#overview)
- [Repo Structure](#repo-structure)
- [Supported Features](#supported-features)
- [Example](#example)
- [Pipeline Architecture](#pipeline-architecture)
- [Macro Expansion: the Hide-Set Algorithm](#macro-expansion-the-hide-set-algorithm)
- [Testing & Validation](#testing--validation)
- [Build & Run](#build--run)
- [License](#license)
- [References](#references)

---

## Overview

`c-preprocess` takes a C source file and resolves everything a real `cpp`
pass would resolve for the features it supports: it splices in `#include`d
files, substitutes `#define`d object-like macros (with correct recursive
expansion and infinite-loop protection via a hide-set algorithm), and
strips `//`/`/* */` comments — all while leaving string and character
literals completely untouched.

It deliberately does **not** implement function-like macros, conditional
compilation (`#ifdef`/`#if`/...), or token concatenation/stringification
(`##`/`#`). Anything in that category is a hard compile error with a clear
`file:line` diagnostic, not a silent no-op — see
[docs/SPEC.md](docs/SPEC.md) for the exact scope and grammar.

`c-compiler`, the sibling subproject in this monorepo, explicitly
treats preprocessing as a non-goal (see its
[ROADMAP.md](../c-compiler/docs/ROADMAP.md)) on the grounds that real
toolchains split `cpp` out as its own pass ahead of the compiler proper.
`c-preprocessor` is that separate pass.

---

## Repo Structure

```
c-preprocessor/
├── README.md
├── LICENSE
├── CMakeLists.txt
├── scripts/
│   └── configure.sh          ← cmake -S . -B build -G Ninja (no external deps)
├── include/
│   ├── token.h                ← PPToken/PPTokenKind/HideSet
│   ├── diagnostics.h          ← PreprocessorError
│   ├── comment_stripper.h
│   ├── pp_tokenizer.h
│   ├── macro_table.h
│   ├── macro_expander.h       ← pure hide-set rescanning
│   └── preprocessor.h         ← directive dispatch + #include recursion
├── src/
│   ├── diagnostics.cpp, comment_stripper.cpp, pp_tokenizer.cpp,
│   │   macro_table.cpp, macro_expander.cpp, preprocessor.cpp
│   └── main.cpp                ← CLI
├── tests/
│   ├── comment_stripper_test.cpp   ← in-memory
│   ├── pp_tokenizer_test.cpp       ← in-memory
│   ├── macro_expander_test.cpp     ← in-memory
│   ├── directive_test.cpp
│   ├── include_resolution_test.cpp
│   ├── preprocessor_test.cpp       ← golden-output comparison
│   ├── cli_test.cpp                ← subprocess exercise of c-preprocess
│   └── fixtures/                   ← small deliberately tricky/broken inputs
├── examples/                   ← realistic multi-file smoke scenario
│   ├── main.c, constants.h, geometry.h, util.h, lib/inner.h
│   └── main.expected.txt       ← checked-in golden output
└── docs/
    └── SPEC.md                 ← directive grammar, comment/macro/include
                                    semantics, worked hide-set examples,
                                    CLI reference, error format
```

---

## Supported Features

**File inclusion** — `#include "filename.h"` (double-quoted only; angle
brackets are a hard error). Resolved relative to the directory of the file
*containing* the `#include`, then against `-I` search directories in the
order given. Circular includes are detected and reported with the full
include chain; diamond includes (the same header reached via two
independent paths) are intentionally **not** deduplicated, since there are
no include guards.

**Object-like macros** — `#define NAME value...` and `#undef NAME`.
Macro bodies are captured as raw tokens at `#define` time and expanded
lazily at each use, with full recursive rescanning (a macro's replacement
can reference another macro) protected against infinite loops by a
hide-set ("blue paint") algorithm — see
[below](#macro-expansion-the-hide-set-algorithm). Macros are never expanded
inside string or character literals.

**Comment stripping** — `//` line comments and `/* */` block comments,
correctly ignored inside string/character literals (and vice versa: quotes
inside comments don't confuse the scanner). A multi-line block comment is
replaced by the same number of newlines it contained, so line numbers in
diagnostics stay accurate across it.

---

## Example

`examples/main.c` includes three headers, uses nested and empty macros,
and mixes both comment styles with a string literal containing `//`:

```c
// c-preprocessor smoke example: comments, macros, and multi-file includes.
#include "constants.h"
#include "geometry.h"
#include "util.h"

/* This block comment
   spans multiple lines
   before the next declaration. */
int main(void) {
    int max = MAX_SCORE;        // MAX_SCORE -> 100
    int min = MIN_SCORE;        // MIN_SCORE -> 0
    int circumference = TWO_PI; // nested macro expansion -> (3 * 2)
    const char *note = "See http://example.com for details"; // // must stay literal
    printf(GREETING);
    printf("%s\n", UNIT_LABEL);
    return max - min - circumference EMPTY_FLAG;
}
```

Running `c-preprocess examples/main.c` produces (see
[examples/main.expected.txt](examples/main.expected.txt) for the full,
checked-in golden output):

```c
int main(void) {
    int max = 100;        
    int min = 0;        
    int circumference = (3 * 2); 
    const char *note = "See http://example.com for details"; 
    printf("Welcome!");
    printf("%s\n", "units");
    return max - min - circumference ;
}
```

Every directive line is consumed (each leaves a blank line behind, which is
why the function body starts partway down the file — see
[docs/SPEC.md](docs/SPEC.md) §2–3 for why directive lines still occupy a
line each), `TWO_PI` expands through `PI` in one nested pass, `UNIT_LABEL`
is pulled in from `lib/inner.h` via a subdirectory-relative include inside
`geometry.h`, and the `http://` inside the string survives untouched.

---

## Pipeline Architecture

```
source file (disk)
        │
        ▼
  CommentStripper        whole-file state machine; strips // and /* */,
                          preserves line count exactly, rejects
                          unterminated comments/literals
        │  comment-free text
        ▼
  Preprocessor            splits into physical lines; dispatches
  (line driver)            #include / #define / #undef, or macro-expands
                          every other line; recurses into CommentStripper
                          + itself for #include, threading ONE shared
                          MacroTable through the whole file tree
        │
        ▼
  PPTokenizer +           pure, in-memory: tokenizes a line, then
  expandMacros             rescans with hide-set tracking so recursive
                          macro references terminate correctly
        │
        ▼
  final preprocessed text  →  stdout or -o file
```

The two hairiest pieces — `PPTokenizer` and `expandMacros`/`expandText` —
are pure functions with no file I/O, which is what makes hide-set
rescanning testable in isolation from the filesystem (see
`macro_expander_test`).

---

## Macro Expansion: the Hide-Set Algorithm

Rescanning a macro's replacement text for further macro references is what
makes `TWO_PI` → `(PI * 2)` → `(3 * 2)` work. Done naively, though, it
loops forever on `#define X X + 1`. This implementation uses the standard
hide-set ("blue paint") technique: every token produced by expanding a
macro `M` carries `M` in its hide set, and an identifier already in its own
hide set is left as literal text instead of being expanded again.

```
#define X X + 1
expand "X":
  X{}      -> lookup succeeds -> push  X{X}  +{X}  1{X}
  X{X}     -> hidden (contains X) -> emit "X"
  +{X}     -> not an identifier -> emit "+"
  1{X}     -> not an identifier -> emit "1"
result: "X + 1"
```

Mutual recursion (`#define A B` / `#define B A`) resolves the same way —
expanding `A` yields the deterministic result `A` (not an error; this
matches real C preprocessors on this input). The full worked examples,
including a three-way cycle, live in [docs/SPEC.md](docs/SPEC.md) §4 and
are exercised directly in `tests/macro_expander_test.cpp`.

---

## Testing & Validation

Seven test executables, built with nothing more than `<cassert>` — no
external test framework, so the build stays hermetic and `ctest` is the
only runner a contributor needs. **All 7 suites pass.**

```
$ ctest --test-dir build --output-on-failure
Test project /path/to/c-preprocessor/build
    Start 1: comment_stripper_test
1/7 Test #1: comment_stripper_test ............   Passed    0.00 sec
    Start 2: pp_tokenizer_test
2/7 Test #2: pp_tokenizer_test ................   Passed    0.00 sec
    Start 3: macro_expander_test
3/7 Test #3: macro_expander_test ..............   Passed    0.00 sec
    Start 4: directive_test
4/7 Test #4: directive_test ...................   Passed    0.00 sec
    Start 5: include_resolution_test
5/7 Test #5: include_resolution_test ..........   Passed    0.00 sec
    Start 6: preprocessor_test
6/7 Test #6: preprocessor_test ................   Passed    0.23 sec
    Start 7: cli_test
7/7 Test #7: cli_test .........................   Passed    0.33 sec

100% tests passed, 0 tests failed out of 7
```

### What each suite covers

| Suite | What it checks |
|---|---|
| `comment_stripper_test` | `//`/`/* */` stripping, comments ignored inside string/char literals and vice versa, multi-line block comments preserving line count, unterminated comment/literal errors |
| `pp_tokenizer_test` | Round-trip token reconstruction, identifier maximal munch, pp-number handling (`0X10` as one token), string/char literal round-trip, multi-character operators split into single-char Punct tokens |
| `macro_expander_test` | Basic/empty/nested substitution, self-referential and mutually-recursive (and 3-way cyclic) macros terminating via hide-sets, macros never expanding inside literals, `MacroTable` define/undefine/redefinition |
| `directive_test` | `#define`/`#undef` parsing (including malformed names and function-like rejection), every unsupported directive erroring by name, bare `#` as a no-op |
| `include_resolution_test` | Resolution relative to the including file's directory (not the top-level file's), `-I` search paths and precedence, circular-include detection with the chain named, diamond includes emitted twice (not deduplicated), malformed/missing/angle-bracket includes |
| `preprocessor_test` | End-to-end run against `examples/main.c`: no directive leaks into output, macro values resolved correctly, a `//`-containing string literal survives, line-number preservation across includes/comments, byte-for-byte golden-output comparison |
| `cli_test` | Subprocess exercise of the built `c-preprocess` binary: missing input, `-o`/stdout output, `-I`, unsupported-directive error formatting, `--help`, unknown flags, multiple positionals |

---

## Build & Run

**Dependencies:** CMake 3.20+, a C++20 compiler, Ninja (recommended). No
external libraries — this project has zero dependencies beyond the C++
standard library.

```bash
./scripts/configure.sh        # configures with Ninja
cmake --build build           # compiles libpp_core.a, c-preprocess, and all tests
ctest --test-dir build --output-on-failure
```

### CLI usage

```bash
# Preprocess to stdout
./build/c-preprocess examples/main.c

# Preprocess to a file
./build/c-preprocess examples/main.c -o out.c

# Add an #include search directory
./build/c-preprocess my_file.c -I third_party/include

# Usage
./build/c-preprocess --help
```

---

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for the full text.

---

## References

**C preprocessor semantics**
- ISO/IEC 9899:2018. *Programming Languages — C* (C17 standard), §6.10 —
  the normative reference for preprocessing directives, macro replacement,
  and `#include` search rules.
- Prosser, Dave. *Macro Expansion Algorithm* (ANSI X3J11 committee
  document, 1986) — the widely cited origin of the hide-set / "blue paint"
  rescanning technique this implementation follows for object-like macros.
- Kernighan, Brian W. *The C Preprocessor: A Rational Perspective* — a
  concise account of what `cpp` does and why it's structured as a separate
  textual pass ahead of the compiler proper.

**This monorepo**
- [`c-compiler`](../c-compiler) — the sibling subproject this
  tool complements; its `docs/ROADMAP.md` explains why preprocessing is
  explicitly out of its scope.
