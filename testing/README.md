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

- **`invalid/`** — snapshot-tests `c-lint` and `c-static-analyzer`'s
  default diagnostic output against frozen `<fixture>.expected.txt`
  files, generalizing the golden-fixture convention already used by
  `c-static-analyzer/tests/golden_test.cpp` into a small suite spanning
  both tools.

  ```bash
  python3 testing/invalid/run_negative_tests.py

  # after deliberately changing a rule's message wording:
  python3 testing/invalid/run_negative_tests.py --update
  ```

Toolchain/execution benchmarks land in a follow-up commit and get their
own section here.
