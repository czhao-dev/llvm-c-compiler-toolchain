# build-tool: Makefile subset specification

This document is the normative reference for exactly which Makefile syntax
and semantics `build-tool` implements. It intentionally implements a small,
well-defined subset of GNU Make rather than growing organically — anything
not listed here is explicitly out of scope for now, and is rejected or
ignored rather than partially/incorrectly supported.

## Syntax

A Makefile is a sequence of lines, each one of:

- **Blank line** — ignored.
- **Comment line** — anything from the first unescaped `#` to the end of
  the line is stripped; a line that's empty after stripping is ignored.
- **Recipe line** — a line starting with a literal tab character. Attaches
  (verbatim, tab stripped, nothing else trimmed) to the most recently
  started rule header. A recipe line with no preceding rule header is a
  parse error (`recipe line has no target`).
- **`.PHONY` declaration** — `.PHONY: name1 name2 ...`. Records each name as
  phony; does not itself create a rule. May appear before or after the rule
  it refers to.
- **Rule header** — `target: prereq1 prereq2 ...`. Starts a new "current"
  rule that subsequent recipe lines attach to. A non-blank, non-tab line
  with no `:` is a parse error (`missing separator ':'`).

## Semantics

- **Default goal**: the first rule (in file order) whose target does not
  start with `.`.
- **Prerequisite resolution**: a name is either the target of a rule, or
  (if not) must exist as a file on disk, in which case it's a leaf
  dependency that's always considered up to date. A name that's neither is
  a `No rule to make target '...'` error.
- **Diamond dependencies**: a name referenced by more than one rule is
  resolved — and built — exactly once; every dependent sees the same
  build outcome.
- **Cycle detection**: a dependency cycle is detected before any recipe
  runs and reported as `Circular dependency dropped: a -> b -> a`.
- **Staleness**: a non-phony target is rebuilt if it doesn't exist yet, if
  any prerequisite is missing, if any prerequisite's mtime is strictly
  newer than the target's, or if any prerequisite was itself just rebuilt
  this run. Otherwise it's left alone (`UpToDate`).
- **`.PHONY` targets** always rebuild, regardless of mtimes.
- **Recipe execution**: each recipe line runs as its own `sh -c '<line>'`
  invocation (so `cd`/shell variables do not persist between lines within
  one rule), executed serially in topological order — a target's own
  recipe never starts before all of its prerequisites have finished. The
  line is echoed to stdout immediately before it runs.
- **Fail-fast / `-k`**: the first non-zero-exit (or unlaunchable) recipe
  line fails its whole rule immediately (later lines in that same recipe
  are not run). Without `-k`, once one target fails or is skipped, every
  target reachable only from that failure is `Skipped` rather than run;
  with `-k`, independent branches (that don't depend on the failed target)
  still build normally.

## Explicit non-goals (not supported)

- Variable expansion (`$(VAR)`) and automatic variables (`$@`, `$<`, `$^`).
- Pattern rules (`%.o: %.c`).
- Built-in Make functions (`$(wildcard ...)`, `$(shell ...)`).
- Line continuation (trailing `\`) — a rule's prerequisite list and each
  recipe line must fit on one physical line.
- `include` directives, conditionals (`ifdef`/`ifeq`), `VPATH`,
  order-only prerequisites (`|`), static pattern rules, double-colon
  rules, target-specific variables.
- `-n` (dry run) and `-f` (Makefile path override) — recognized on the
  command line only so they can be rejected with a clear error instead of
  being silently misread as a target name.
- Parallel execution (`-j`) — recipes run serially, one at a time, in
  topological order. This only affects wall-clock speed, not correctness;
  see the README for the rationale.
