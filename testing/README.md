# testing

Cross-subproject test and benchmark suites, kept outside all 6 subprojects
since each of those is independent and self-contained (its own tests/,
build system, README) and none of them describes a dependency on another.
This directory is the one place that's explicitly allowed to depend on
several subprojects' built binaries at once.

All scripts here are plain Python 3 (standard library only, unless a
script's own docstring says otherwise) and assume the relevant
subprojects are already built via their own `./scripts/configure.sh &&
cmake --build build` — nothing in `testing/` builds anything itself.

## Suites

- **`differential/`** — compiles each `.mc` case with both `minic` and
  `clang -x c`, runs the resulting binaries, and asserts stdout + exit
  code match byte-for-byte at `-O0` and `-O2`.

  ```bash
  python3 testing/differential/run_differential_tests.py
  ```

More suites (negative/snapshot testing of the linter and analyzer's
guardrails, and toolchain/execution benchmarks) land in follow-up commits
and get their own section here.
