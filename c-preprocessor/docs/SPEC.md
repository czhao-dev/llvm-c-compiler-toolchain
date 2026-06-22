# c-preprocessor Specification

## 1. Scope

Supported:

- `#include "filename.h"` — quoted file inclusion.
- `#define NAME replacement-tokens...` — object-like macros (rest-of-line
  text substitution, no parameters).
- `#undef NAME` — removes a macro definition.
- Comment stripping: `//` line comments and `/* */` block comments.

Explicitly **not** supported — each produces a hard error rather than being
silently ignored or mishandled:

- Function-like macros (`#define SQUARE(x) ((x)*(x))`).
- Conditional compilation (`#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`,
  `#endif`).
- Macro concatenation (`##`) and stringification (`#`) — these never arise
  as their own directives, but a function-like `#define` (the only place
  they could appear) is itself rejected.
- Angle-bracket includes (`#include <file.h>`).
- Backslash-newline line continuation (splicing). Real C performs this as a
  pass over the whole file *before* comment stripping and directive
  parsing; retrofitting it would conflict with this implementation's core
  assumption that one directive occupies exactly one physical line.
- Any other `#`-directive (`#pragma`, `#error`, `#line`, or anything
  unrecognized) is a hard error naming the directive.

## 2. Directive grammar

```
directive       ::= '#' ws* (include-directive | define-directive | undef-directive)?
include-directive ::= 'include' ws+ '"' filename '"'
define-directive   ::= 'define' ws+ identifier (ws+ replacement-tokens?)?
undef-directive     ::= 'undef' ws+ identifier
```

- A bare `#` (optionally followed only by whitespace) is a silent no-op —
  it still occupies one output line (a blank line), matching how a real
  preprocessor's null directive behaves.
- Leading whitespace before the `#` is permitted.
- `#define NAME` with no replacement text defines `NAME` as replacing to
  nothing (empty macro).
- A `#define` whose name is immediately followed by `(` (no space) is a
  function-like macro definition and is rejected: `"function-like macros
  are not supported: 'NAME'"`.
- Any other directive name is rejected: `"unsupported preprocessor
  directive '#name'"`.

## 3. Comment-stripping rules

A single left-to-right pass over the whole file, before any directive or
macro processing, with a five-state machine: `Normal`, `LineComment`,
`BlockComment`, `InString`, `InChar`.

- `//` and `/* */` are only recognized in `Normal` state — never inside a
  string or character literal (`"a//b"`, `'/'`, `"/*"` are untouched).
- `"` and `'` are only recognized in `Normal` state — never inside a
  comment (`// still a comment 'til the newline`, `/* has "quotes" inside */`
  are handled correctly).
- A block comment spanning multiple physical lines is replaced by exactly
  as many `\n` characters as it contained, so every line **after** the
  comment keeps its original 1-based line number for diagnostics.
- An unterminated block comment, or an unterminated string/character
  literal (a raw newline before the closing quote), is a hard error
  attributed to the line the comment/literal started on.
- A `//` comment running off the end of the file (no trailing newline)
  is not an error.

**Known limitation:** because comment stripping runs before directives are
parsed, and a stripped block comment's internal newlines become real line
breaks in the stripped text, a block comment that spans the *middle* of a
`#define`/`#include`/`#undef` line will truncate that directive early. This
is rare in practice and is an accepted consequence of not supporting
backslash-newline splicing.

## 4. Macro semantics

- Object-like only: a `#define`'s replacement is the (tokenized) rest of
  the line, with no parameter list.
- Macro bodies are stored as **raw, unexpanded tokens** captured at
  `#define` time, and only expanded lazily when a use is rescanned. This
  gives `#undef` correct late-binding behavior for free: if `B` is defined
  in terms of `A` and `A` is later `#undef`'d before `B` is used, `B`
  expands to the literal, no-longer-defined `A` (not to `A`'s old value).
- Redefining a macro overwrites the previous definition — last-wins, with
  no diagnostic. This is a deliberate simplification versus strict C
  (which requires identical redefinition or a diagnostic).
- Macro expansion never occurs inside string or character literals.

### Rescanning and the hide-set algorithm

A macro's replacement text can reference other macros, and that reference
must itself be expanded (rescanned) — but naively rescanning forever would
infinite-loop on self-referential or mutually recursive macros. This
implementation uses the standard hide-set ("blue paint") technique: every
time a macro `M` is expanded, every token produced by that expansion
carries `M` in its hide set. If, during rescanning, an identifier equal to
one of the names in its own hide set is encountered, it is left as literal
text instead of being expanded again.

**Self-reference** — `#define X X + 1`, expanding `X`:

| step | token | hide set | action |
|---|---|---|---|
| 1 | `X` | `{}` | not hidden; expands to `X`, `+`, `1`, each tagged `{X}` |
| 2 | `X` | `{X}` | hidden → emitted literally |
| 3 | `+` | `{X}` | not an identifier → emitted |
| 4 | `1` | `{X}` | not an identifier → emitted |

Result: `X + 1`.

**Mutual recursion** — `#define A B` / `#define B A`, expanding `A`:

| step | token | hide set | action |
|---|---|---|---|
| 1 | `A` | `{}` | expands to `B`, tagged `{A}` |
| 2 | `B` | `{A}` | not hidden (only `A` is hidden so far); expands to `A`, tagged `{A,B}` |
| 3 | `A` | `{A,B}` | hidden → emitted literally |

Result: `A`. This is a deterministic, well-defined outcome that matches
real C preprocessors on this input — it is **not** treated as an error.
Expanding `B` instead is symmetric and yields literal `B`.

This also guarantees termination in general: a token's hide set can only
grow, and the macro table is finite, so a token's hide set can accumulate
at most one entry per macro in the table before it necessarily contains
every macro name that could still match it. A defensive token-count budget
exists purely as a guard against a hide-set bookkeeping bug; it should
never trigger on correct input.

## 5. Tokenization for macro expansion

Applied to a single already-comment-stripped line (or a `#define`
replacement fragment):

- **Identifier**: `[A-Za-z_][A-Za-z0-9_]*`, maximal munch — `XY` is always
  one token, never `X` followed by `Y`.
- **Number** (a deliberately permissive "pp-number"): a digit followed by
  any run of letters/digits/`_`/`.`. This over-matches real numeric-literal
  grammar on purpose, so `0X10` is always one Number token — a macro
  literally named `X` must never match inside it.
- **String / char literal**: consumed atomically including `\`-escapes;
  never scanned for macro names inside.
- **Whitespace**: a run of spaces/tabs becomes one token (this is what
  preserves original spacing in the output).
- Everything else (operators, punctuation) is consumed **one character at
  a time** — multi-character operators like `->`, `==`, `<<` are never
  recognized as a unit, because the tool only needs to leave them
  untouched, never interpret them.

## 6. `#include` resolution

Search order for `#include "filename"`:

1. The directory containing the file that has the `#include` directive
   (quote-include semantics — *not* the top-level input file's directory,
   and *not* the process's working directory).
2. Each `-I <dir>` command-line flag, in the order given.
3. Not found anywhere → error listing every directory searched.

Circular includes are detected via a stack of canonicalized absolute paths
for the **currently active** include chain (innermost last) — not a global
"ever included" set. A file that transitively includes itself produces an
error naming the include chain. Because there are no include guards (no
conditional compilation), a **diamond** include — the same header reached
via two independent, non-cyclic paths — is intentionally **not**
deduplicated: its content is processed and emitted once per path.

`#include <file.h>` (angle brackets) is rejected with a clear error
pointing at `"..."` as the supported form.

## 7. CLI reference

```
c-preprocess <input-file> [-I <dir>]... [-o <output-file>] [-h|--help]
```

| Flag | Behavior |
|---|---|
| `<input-file>` | required positional; the file to preprocess |
| `-I <dir>` | add a directory to the `#include` search path; repeatable |
| `-o <file>` | write output to `<file>` instead of stdout |
| `-h`, `--help` | print usage, exit 0 |

Exit codes: `0` success, `1` the tool ran and failed (preprocessing error,
I/O error, malformed flag), `2` no input file was given (usage error).

## 8. Error message format

`file:line: error: message`, e.g.:

```
main.c:12: error: unsupported preprocessor directive '#ifdef'
constants.h:3: error: cannot find include file "missing.h" (searched: constants.h/../missing.h)
a.h:1: error: circular #include detected: a.h -> b.h -> a.h
```

When no specific line applies (e.g. the top-level input file can't be
opened at all), the format drops the line number: `file: error: message`.

## 9. Known limitations / non-goals

Backslash-newline line continuation, angle-bracket includes, conditional
compilation, function-like macros, and `##`/`#` are all out of scope (see
§1). Macro redefinition is last-wins with no compatibility check (§4).
