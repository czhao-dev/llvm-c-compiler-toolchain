# c-static-analyzer: rule semantics specification

This document is the normative reference for exactly what each rule flags
and how the shared machinery (file discovery, config, diagnostics) behaves.

## Diagnostics

A `Diagnostic` has `path`, `line` (1-indexed), `col` (0-indexed, tracked
but never printed), `ruleId`, and `message`. Output is rendered as exactly
`{path}:{line}: {ruleId} {message}` — no brackets, no column. All
diagnostics from a scan are sorted once, globally, by
`(path, line, col, ruleId, message)`.

An unreadable file yields a single synthetic `SA000` diagnostic
(`"Could not read file: <reason>"`) instead of aborting the whole scan.
`SA000` is not a real rule — it's not registered, can't be selected or
excluded, and exists only for this I/O-failure path.

## File discovery

- Extensions: only `.c` and `.h` files are considered.
- Default-excluded directory names (matched as whole path components, not
  patterns): `.git`, `build`, `dist`, `cmake-build-debug`,
  `cmake-build-release`, `CMakeFiles`, `out`, `vendor`, `third_party`.
- User-supplied `--exclude`/config `exclude` patterns are glob patterns
  (see below), matched against both the full posix-normalized path and the
  bare filename — either match excludes the file.
- A directory argument is walked recursively (exclusion is a post-filter,
  not applied during the walk itself) and its matches are sorted before
  filtering. A file argument given directly is included only if it has a
  `.c`/`.h` extension and isn't excluded.

## Glob matching (fnmatch)

A small, case-sensitive, fully-anchored port of Python's
`fnmatch.fnmatch`: `*` (zero or more of anything), `?` (exactly one
character), `[seq]` / `[!seq]` (a character class, negatable, supporting
`a-z`-style ranges). No backslash escaping. A `[` with no matching `]`
anywhere in the pattern is treated as a literal `[`.

## Config file

`.c-static-analyzer.toml`, searched from the scan's working directory
upward through ancestor directories — the nearest one found wins (even if
it fails to parse, in which case defaults are used rather than continuing
to search further up). Recognized keys, all optional:

```toml
exclude        = ["tests/fixtures/*"]  # default: []
max_complexity = 10                    # default: 10
max_nesting    = 4                     # default: 4
enabled_rules  = ["SA001", "SA002"]    # default: [] (all enabled)
```

CLI flags are applied on top, in this order: `--max-complexity`,
`--max-nesting`, `--select` (replaces `enabled_rules` wholesale),
`--exclude` (extends the config file's `exclude` list, doesn't replace it).

## SA001 — Complexity

McCabe-style cyclomatic complexity: start at 1, then walk the entire
function body adding 1 for each `if`/`for`/`while`/`do`/ternary
(`?:`), each `&&`/`||` binary operator, and each `case` label that has a
value (a bare `default:` does **not** count). Flags a function whose total
exceeds `max_complexity`. Every function in a file is scored independently.

## SA002 — Unused variables

Considers only true locals: names introduced by a `declaration` node
inside a function's body (not parameters, not globals). A name starting
with `_` is exempt (an explicit "intentionally unused" convention). A
variable counts as "used" if it's referenced anywhere except: (a) its own
declaration site, or (b) as the plain left-hand side of a bare `=`
assignment — a compound assignment like `x += 1` still counts as a use,
since it also reads `x`. Array-size expressions and initializers count as
uses of the variables they reference.

## SA003 — Nesting

Walks `if`/loop/`switch` statements per function, starting at depth 0 and
incrementing on each one. Reports only the **first** violation per
function. An `else if` continues the same `if` chain at the *same* depth
(it does not add a level); a genuine `else { ... }` block adds one level,
same as any other nested block. Statements directly inside a `case` label
are walked at the switch's own (incremented) depth, not one level deeper.

## SA004 — Missing return

Skipped entirely for `void`-returning functions (a function returning
`void*` is not considered void). A function's last statement must
guarantee a value is returned for the function to pass:

- `return ...;` always exits.
- `if`/`else` only exits if **both** branches always exit (recursively —
  this also handles `else if` chains, since the `else` branch may itself
  be another `if`). An `if` with no `else` never counts, since there's
  always a fall-through path.
- `while (1)` / `do { } while (1)` (literal `1` or `true` as the
  condition, after trimming parens/spaces — not general constant folding)
  count as always exiting, **provided** there's no `break` targeting that
  same loop (a `break` inside a *nested* loop or `switch` doesn't count,
  since it belongs to that inner construct instead).
- Anything else (including a `for` loop, or a bare `switch`, as the last
  statement) does not guarantee an exit.

## SA005 — Unreachable code

For every block (a `compound_statement`, or the statements inside one
`case` label, excluding the `case`'s own value expression), if any
statement other than the last is a `return`/`break`/`continue`/`goto`,
the very next statement is flagged as unreachable. Only the **first**
such occurrence per block is reported — this rule doesn't try to flag
every subsequent statement, just the first sign of dead code — though
nested blocks are each checked independently, so dead code inside a
nested `if`/loop is still caught.
