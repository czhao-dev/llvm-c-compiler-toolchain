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

SA004, SA005, and SA006 are all built on a common control-flow graph (CFG)
module (`include/cfg.h`/`src/cfg.cpp`): a basic-block/edge graph is built
per function body (handling if/else, while/do-while/for — including
literal-`1`/`true` infinite-loop detection — switch/case fallthrough,
break/continue, and goto/labels), and each rule is a traversal over that
graph rather than an ad hoc recursive walk of the AST. This is a genuine
improvement over each rule's previous AST-pattern-matching implementation,
not just a refactor — see each section below for what specifically got
more correct.

## SA004 — Missing return

Skipped entirely for `void`-returning functions (a function returning
`void*` is not considered void). For every other function, its CFG's
synthetic exit block is inspected: if exit has an incoming *Fallthrough*
edge (as opposed to the *Unconditional* edge every `return` statement
produces) from a block reachable from entry, the function can fall off the
end without returning a value, and is flagged.

This is exactly equivalent to asking "is there a path from entry to the
end of the function that never passes through a `return`?", computed
structurally instead of by pattern-matching the last statement's shape.
`while (1)`/`do { } while (1)` (literal `1`/`true` condition, trimmed of
parens/spaces — not general constant folding) correctly has no Fallthrough
edge out of the loop at all unless a `break` reaches past it, so an
infinite loop without a reachable `break` is still recognized as always
exiting, and one with a `break` is not — the same behavior as before, now
falling out of the graph structure rather than a special-cased check.

## SA005 — Unreachable code

Any CFG block containing statements that is *not* reachable from the
function's entry block is flagged, reported at its first statement.
Because reachability is a real graph property instead of "is the previous
statement in the same source block a `return`/`break`/`continue`/`goto`",
this correctly generalizes to cases the old adjacent-statement scan
couldn't see — for example, an entire `if`-branch dead after a prior
unconditional `return` in a sibling branch, or a loop's own update clause
when the body unconditionally `break`s before ever reaching it. The
diagnostic message still names the specific keyword (`return`/`break`/
`continue`/`goto`) when the unreachable block's first statement directly
follows one of them in the same source block, falling back to a generic
"Unreachable code" when the cause isn't that simple (e.g. dead code after
an exhaustive `if`/`else`).

## SA006 — Uninitialized variable

Considers the same population of locals as SA002 (a `declaration` inside a
function body, name not starting with `_`), restricted to locals declared
**without** an initializer. For each such local, this rule runs a forward
"may be uninitialized" dataflow analysis over the CFG, starting at the
declaration: a write is either the plain left-hand side of a bare `=`
assignment (`x = 1;`), or a `.`/`->` field access that is itself the plain
left-hand side of a bare `=` assignment (`pt.x = 1;` / `p->x = 1;` —
so the declare-then-initialize-field-by-field idiom isn't flagged);
anything else — a plain read, a function-call argument, a condition, an
address-of (`&x`), or a compound-assignment target like `x += 1` (which
also reads `x`) — is a read. The local is flagged at the first read
reachable from its declaration via some path that never passes through a
write first — an existential ("may", not "must") check, the same shape a
real compiler's uninitialized-variable warning uses, so a variable that's
only initialized on *some* branches is still correctly flagged even if a
different branch happens to write it first in source order.

If a local is never referenced again at all, SA006 does not flag it —
that's SA002's job, not SA006's; the two rules never double-report the
same declaration.

Moving to real dataflow fixes a genuine class of false negative the old
single-textual-pass version had — `int x; if (c) { x = 1; } return x;` is
now correctly flagged (uninitialized whenever `c` is false), where the old
implementation saw the write inside the `if` as the first textual
reference and never flagged it. Two limitations carry over unchanged,
since they're about how a *write* is classified rather than about control
flow, which dataflow alone doesn't fix:

- False positive: `int x; scanf("%d", &x); use(x);` is flagged — `&x` is
  classified as a read (it isn't the left-hand side of `=`), even though
  it's actually being filled in by `scanf`. Passing an uninitialized local
  by address to be written through it is a real, known limitation, not an
  edge case worth silently ignoring in this note.
- Shadowing: since neither SA002 nor SA006 is scope-aware (both use a flat
  per-function name→declaration-site map, first declaration wins), a
  reference to an inner shadowing declaration of the same name may be
  attributed to the outer one instead.

**Array-typed locals are not analyzed at all.** `arr[0] = 5;` parses so
that `arr`'s immediate parent is a `subscript_expression`, not an
`assignment_expression` — a write through a subscript would always be
misclassified as a read by the rule above, so array-typed declarations are
excluded entirely rather than guaranteed false-positive on nearly every
declared-then-indexed array. Subscript-aware write detection is an
explicit non-goal.
