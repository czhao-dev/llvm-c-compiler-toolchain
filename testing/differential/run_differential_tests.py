#!/usr/bin/env python3
"""Differential test runner.

For each testing/differential/cases/*.mc file, compiles it with both
`minic` and `clang -x c` at -O0 and -O2, runs the resulting binaries, and
asserts stdout + exit code match byte-for-byte.

`.mc` is a genuine subset of C (see c-compiler/docs/language_spec.md),
so clang can compile the same source directly via `-x c`, which forces
C-mode parsing regardless of the .mc extension -- no copying/symlinking to
a .c extension needed.

Exit code: 0 if every case passes at every optimization level, 1 otherwise.
"""

import difflib
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import binaries  # noqa: E402
from common.proc import run  # noqa: E402

CASES_DIR = Path(__file__).resolve().parent / "cases"
OPT_LEVELS = ["-O0", "-O2"]


def find_clang() -> str:
    clang = shutil.which("clang")
    if clang is None:
        print("error: clang not found on PATH", file=sys.stderr)
        sys.exit(2)
    return clang


def run_case(case_path: Path, minic_bin: Path, clang_bin: str, opt: str, tmpdir: Path) -> list:
    """Compiles and runs `case_path` with both toolchains at `opt`.
    Returns a list of failure messages (empty if the case passed)."""
    minic_exe = tmpdir / f"{case_path.stem}_minic{opt}"
    clang_exe = tmpdir / f"{case_path.stem}_clang{opt}"

    minic_compile = run([str(minic_bin), str(case_path), opt, "-o", str(minic_exe)], timeout=60)
    if minic_compile.exit_code != 0:
        return [f"{case_path.name} {opt}: minic failed to compile:\n{minic_compile.stderr}"]

    # MiniC examples call printf() with no #include <stdio.h> (it has no
    # standard-library headers). Recent clang treats a known library
    # function called without a prototype as a hard error unconditionally
    # (not just under newer -std defaults), so it has to be silenced
    # explicitly rather than worked around with -std alone.
    clang_compile = run(
        [
            clang_bin,
            "-x",
            "c",
            "-std=c99",
            "-Wno-implicit-function-declaration",
            "-Wno-implicit-int",
            opt,
            str(case_path),
            "-o",
            str(clang_exe),
        ],
        timeout=60,
    )
    if clang_compile.exit_code != 0:
        return [f"{case_path.name} {opt}: clang failed to compile:\n{clang_compile.stderr}"]

    minic_run = run([str(minic_exe)], timeout=10)
    clang_run = run([str(clang_exe)], timeout=10)

    failures = []

    if minic_run.exit_code != clang_run.exit_code:
        failures.append(
            f"{case_path.name} {opt}: exit code mismatch "
            f"(minic={minic_run.exit_code}, clang={clang_run.exit_code})"
        )

    if minic_run.stdout != clang_run.stdout:
        diff = "".join(
            difflib.unified_diff(
                clang_run.stdout.splitlines(keepends=True),
                minic_run.stdout.splitlines(keepends=True),
                fromfile="clang stdout",
                tofile="minic stdout",
            )
        )
        failures.append(f"{case_path.name} {opt}: stdout mismatch\n{diff}")

    return failures


def main() -> int:
    try:
        minic_bin = binaries.minic()
    except binaries.BinaryNotBuiltError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    clang_bin = find_clang()

    cases = sorted(CASES_DIR.glob("*.mc"))
    if not cases:
        print(f"error: no .mc cases found in {CASES_DIR}", file=sys.stderr)
        return 2

    all_failures = []
    with tempfile.TemporaryDirectory(prefix="minic_differential_") as tmp:
        tmpdir = Path(tmp)
        for case_path in cases:
            for opt in OPT_LEVELS:
                failures = run_case(case_path, minic_bin, clang_bin, opt, tmpdir)
                if failures:
                    all_failures.extend(failures)
                else:
                    print(f"PASS  {case_path.name} {opt}")

    if all_failures:
        print("\n=== FAILURES ===\n")
        for failure in all_failures:
            print(failure)
            print()
        print(f"{len(all_failures)} failure(s)")
        return 1

    print(f"\nAll {len(cases) * len(OPT_LEVELS)} differential cases passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
