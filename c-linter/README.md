# c-linter

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

> A style/formatting linter for C: snake_case naming, line length and
> trailing whitespace, magic-number detection in comparisons, and K&R/Allman
> brace-style consistency. Reports warnings only — no auto-fixing, and no
> semantic analysis (that boundary belongs to `c-static-analyzer`).

---

## Table of Contents

- [Overview](#overview)
- [Repo Structure](#repo-structure)
- [Rules](#rules)
- [Example](#example)
- [Architecture](#architecture)
- [Testing](#testing)
- [Build & Run](#build--run)
- [License](#license)
- [References](#references)

---

## Overview

`c-lint` checks C source files against four style conventions and reports
one warning per violation, in source order, to stdout. It intentionally
does **not**:

- Auto-fix or reformat code — finding a style issue and fixing it are very
  different problems; this tool only does the former.
- Perform any semantic analysis (unused variables, shadowing, unreachable
  code, ...) — that's `c-static-analyzer`'s job, a sibling subproject in
  this monorepo. `c-linter` only cares about syntax aesthetics.
- Track indentation depth — deceptively fiddly across nested blocks and
  deliberately out of scope for this MVP.

It ships its own small, tolerant lexer rather than depending on
`c-compiler`'s, keeping the subproject independent per this repo's
convention that each piece has its own language, build system, tests, and
README.

---

## Repo Structure

```
c-linter/
├── README.md
├── LICENSE
├── CMakeLists.txt
├── scripts/
│   └── configure.sh          ← cmake -S . -B build -G Ninja (no external deps)
├── include/
│   ├── token.h                ← TokenType/SourceLocation/Token
│   ├── lexer.h                ← tolerant, never-throws Lexer
│   ├── diagnostic.h           ← Severity/RuleCode/Diagnostic/format()
│   ├── line_rules.h           ← CL002/CL003, raw-text pass
│   ├── naming_rule.h          ← CL001
│   ├── magic_number_rule.h    ← CL004
│   ├── brace_style_rule.h     ← CL005, BraceStyle
│   ├── linter.h               ← LinterOptions/Linter orchestration
│   └── snippet.h               ← --show-source caret/snippet rendering
├── src/
│   ├── token.cpp, lexer.cpp, diagnostic.cpp, line_rules.cpp,
│   │   naming_rule.cpp, magic_number_rule.cpp, brace_style_rule.cpp,
│   │   linter.cpp, snippet.cpp
│   └── main.cpp                ← CLI
├── tests/
│   ├── lexer_test.cpp, naming_rule_test.cpp, line_rules_test.cpp,
│   │   magic_number_rule_test.cpp, brace_style_rule_test.cpp,
│   │   snippet_test.cpp        ← in-memory
│   ├── linter_test.cpp         ← fixture/example-based integration test
│   ├── cli_test.cpp            ← subprocess exercise of c-lint
│   └── fixtures/               ← kitchen_sink.c, clean.c, allman_style.c, short_line.c
├── examples/
│   └── sample.c                ← realistic worked example
└── docs/
    └── SPEC.md                 ← rule definitions, lexer tolerance policy,
                                    exemption rationale
```

---

## Rules

| Code | Check | Default |
|---|---|---|
| `CL001` | Naming convention (snake_case) | — |
| `CL002` | Line length | 80 characters (`--max-line-length=N`) |
| `CL003` | Trailing whitespace | — |
| `CL004` | Magic number in a comparison | — |
| `CL005` | Brace-style consistency | K&R (`--brace-style=kr\|allman`) |

**CL001 — naming.** Every `Identifier` token is flagged if it contains an
uppercase letter *and* no underscore — `totalCount` and `PI` are flagged;
`total_count` and `MAX_SIZE` are not. This is a token-level check with no
symbol table, so every *occurrence* of a badly-named identifier is flagged
independently, not just its declaration — see
[docs/SPEC.md](docs/SPEC.md) for why that's a deliberate simplification
rather than a bug.

**CL002 / CL003 — line length and trailing whitespace.** A pure raw-text
pass over the file, run before tokenization: each line longer than the
configured limit is flagged (CL002), and each line ending in a space or tab
is flagged (CL003) — independently, so one line can emit both.

**CL004 — magic numbers.** A comparison operator (`==`, `!=`, `<`, `>`,
`<=`, `>=`) immediately followed by an integer literal is flagged, with a
suggestion to use a named macro instead. `0`, `1`, and `-1` are exempt as
common sentinel values (`!= 0`, `>= 1`, `!= -1`) that aren't meaningfully
"magic."

**CL005 — brace style.** For each `if`/`while`, the closing parenthesis of
its condition (correctly matched through nested parens) is compared against
the opening brace that immediately follows it: K&R requires the same line,
Allman requires the brace on its own line. Bodies without a brace at all
(`if (x) foo();`) are out of scope — this rule only compares placement when
a brace is actually present.

---

## Example

```c
void stack_push(struct Stack *s, int value) {
    if (s->topIndex >= 31)
    {
        return;
    }
    ...
}
```

```
$ c-lint examples/sample.c
examples/sample.c:5: warning: identifier 'Stack' should be snake_case [CL001]
examples/sample.c:7: warning: identifier 'topIndex' should be snake_case [CL001]
examples/sample.c:11: warning: identifier 'topIndex' should be snake_case [CL001]
examples/sample.c:11: warning: magic number '31' in comparison; consider a named macro [CL004]
examples/sample.c:12: warning: opening brace for 'if' does not match configured K&R style [CL005]
...
```

The full file is [examples/sample.c](examples/sample.c); running it through
`c-lint` also flags every later use of `topIndex`/`myStack` (CL001 is
per-occurrence, as noted above), which is why the real output is longer
than this excerpt.

---

## Architecture

```
source file (disk)
        │
        ▼
  checkLineRules()       raw-text pass: line length (CL002) and
                          trailing whitespace (CL003), before tokenization
        │
        ▼
  Lexer::tokenize()       tolerant lexer: never throws on malformed or
                          unmodeled input, only recognizes what the 4
                          rules need (if/while, comparisons, parens/braces,
                          literals), everything else -> Other
        │
        ├─→ checkNamingRule()        (CL001)
        ├─→ checkMagicNumberRule()   (CL004)
        └─→ checkBraceStyleRule()    (CL005)
        │
        ▼
  Linter::lintSource()    aggregates + sorts all diagnostics by (line, column)
```

`Linter::lintSource()` is the only public entry point and is pure (no file
I/O), so every rule is directly testable on in-memory source strings. File
I/O and "cannot open" reporting live in `main.cpp`. See
[docs/SPEC.md](docs/SPEC.md) for the full rule definitions, the lexer's
tolerance policy, and the reasoning behind each exemption.

---

## Testing

Eight test executables, built with nothing more than `<cassert>`/manual
assertions — no external test framework. **All 8 suites pass.**

```
$ ctest --test-dir build --output-on-failure
Test project .../c-linter/build
    Start 1: lexer_test
1/8 Test #1: lexer_test .......................   Passed
    Start 2: naming_rule_test
2/8 Test #2: naming_rule_test .................   Passed
    Start 3: line_rules_test
3/8 Test #3: line_rules_test ..................   Passed
    Start 4: magic_number_rule_test
4/8 Test #4: magic_number_rule_test ...........   Passed
    Start 5: brace_style_rule_test
5/8 Test #5: brace_style_rule_test ............   Passed
    Start 6: linter_test
6/8 Test #6: linter_test ......................   Passed
    Start 7: snippet_test
7/8 Test #7: snippet_test .....................   Passed
    Start 8: cli_test
8/8 Test #8: cli_test .........................   Passed

100% tests passed, 0 tests failed out of 8
```

| Suite | What it checks |
|---|---|
| `lexer_test` | Keyword/identifier classification, int vs. float literal forms, string/char literal handling, comment stripping with line tracking, unterminated comment/string/char never crashing, all 6 comparison operators, arbitrary punctuation falling through to `Other` |
| `naming_rule_test` | camelCase/PascalCase/ALLCAPS flagged, snake_case and underscored ALL_CAPS not flagged, real keywords never flagged |
| `line_rules_test` | Length boundary at the configured limit, custom limits, trailing space/tab, a line hitting both codes at once, CRLF safety, final line without a trailing newline |
| `magic_number_rule_test` | All 6 comparison operators, the 0/1/-1 exemption, hex/octal parsed by value not string form, suffixed literals, non-adjacent and non-integer cases left alone |
| `brace_style_rule_test` | K&R and Allman in both directions, nested parens in the condition, braceless bodies and `do`/`while` trailers left alone, truncated input not crashing |
| `linter_test` | Full aggregation/sort contract on a fixture hitting all 5 codes, zero diagnostics on a clean fixture, `LinterOptions` overrides changing results, structural checks against `examples/sample.c` |
| `snippet_test` | Caret column alignment (including column 1), `formatWithSource`'s full header+snippet output |
| `cli_test` | Subprocess exercise of the built `c-lint` binary: usage/exit codes, multi-file runs, a missing file alongside a valid one, `--max-line-length`, `--brace-style`, `--show-source`, `--help`, malformed flags |

---

## Build & Run

**Dependencies:** CMake 3.20+, a C++20 compiler, Ninja (recommended). No
external libraries.

```bash
./scripts/configure.sh        # configures with Ninja
cmake --build build           # compiles libcl_core.a, c-lint, and all tests
ctest --test-dir build --output-on-failure
```

### CLI usage

```bash
# Lint one or more files, printing diagnostics to stdout
./build/c-lint file1.c file2.c

# Override the line-length limit
./build/c-lint file.c --max-line-length=100

# Use Allman brace style instead of the K&R default
./build/c-lint file.c --brace-style=allman

# Also print the offending source line and a caret under each diagnostic
./build/c-lint file.c --show-source

# Usage
./build/c-lint --help
```

`--show-source` is purely additive — it appends a two-line snippet under
each diagnostic without changing the default single-line output, so
scripts parsing the default format are unaffected:

```
$ c-lint examples/sample.c --show-source
examples/sample.c:5:8: warning: identifier 'Stack' should be snake_case [CL001]
struct Stack {
       ^
```

Exit codes: `0` if every file opened and no diagnostics were produced,
`1` if any file failed to open or any diagnostic was emitted, `2` for a
usage error (no input files given).

---

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for the full text.

---

## References

- Kernighan, Brian W. and Ritchie, Dennis M. *The C Programming Language*
  (2nd ed.). Prentice Hall, 1988. — the source of the "K&R" brace-style
  terminology this tool's default configuration is named after.
- [c-static-analyzer](../c-static-analyzer) — the sibling subproject that
  owns semantic checks (unused variables, nesting depth, missing returns,
  ...); `c-linter` deliberately stays out of that territory.
