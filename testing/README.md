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

- **`benchmarks/`** — toolchain compile-speed and execution-speed
  comparison between `minic` and `clang`, using
  [hyperfine](https://github.com/sharkdp/hyperfine) (a hard dependency
  for this suite specifically — `brew install hyperfine` locally,
  installed explicitly in CI). Programs live in `benchmarks/programs/`:
  `sieve.mc`, `fibonacci.mc` (naive recursive), `bubble_sort.mc`
  (worst-case reverse-sorted input).

  ```bash
  python3 testing/benchmarks/run_benchmarks.py --output testing/benchmarks/results.json
  python3 testing/benchmarks/report.py testing/benchmarks/results.json
  python3 testing/benchmarks/plot.py testing/benchmarks/results.json   # requires matplotlib, see requirements.txt
  ```

  `results.json` is a point-in-time measurement and is gitignored; the
  generated chart PNGs under `benchmarks/charts/` are committed, since
  they're the artifacts the root README embeds.

  This suite's own exploration surfaced two real, pre-existing bugs in
  `c-compiler-llvm`, documented where discovered rather than fixed here
  (out of scope for a testing/benchmarking task): see the header comments
  in `testing/differential/cases/03_pointers.mc` (an `-O2` codegen bug
  for functions taking an `int **` parameter) and
  `testing/benchmarks/programs/sieve.mc` (a runtime crash and, separately,
  an `-O2` compile-time hang, both tied to a loop nested inside another
  loop at moderate-to-large iteration counts). `run_benchmarks.py`
  survives the hang by bounding every hyperfine invocation with an
  external timeout and recording that combination as a failure rather
  than blocking the rest of the suite.
