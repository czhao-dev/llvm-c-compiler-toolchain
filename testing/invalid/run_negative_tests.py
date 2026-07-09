#!/usr/bin/env python3
"""Negative/snapshot test runner.

For each (fixture, expected) pair below, runs the relevant tool against
the fixture -- cwd'd into testing/invalid/ so the diagnostic paths in the
tool's output are just the bare filename, independent of the caller's
cwd -- and compares stdout byte-for-byte against a frozen
`<fixture>.expected.txt` file. This generalizes the same golden-fixture
convention c-static-analyzer's own tests/golden_test.cpp already uses
into a small suite spanning multiple tools.

Snapshots use each tool's *default* output (no --show-source), keeping
this suite decoupled from the caret-rendering feature; a --show-source
fixture can be added separately later.

Pass --update to regenerate the .expected.txt files from actual output
(useful once a rule's exact message wording is finalized).

Exit code: 0 if every fixture matches its snapshot, 1 otherwise.
"""

import argparse
import difflib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import binaries  # noqa: E402
from common.proc import run  # noqa: E402

FIXTURES_DIR = Path(__file__).resolve().parent


def _cases(c_lint_bin: Path, c_static_analyzer_bin: Path):
    """Each entry: (fixture filename, expected filename, command)."""
    return [
        ("fail_snake_case.c", "fail_snake_case.expected.txt", [str(c_lint_bin), "fail_snake_case.c"]),
        (
            "fail_uninitialized.c",
            "fail_uninitialized.expected.txt",
            [str(c_static_analyzer_bin), "scan", "--no-config", "--select", "SA006", "fail_uninitialized.c"],
        ),
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--update", action="store_true", help="regenerate .expected.txt files from actual output")
    args = parser.parse_args()

    try:
        c_lint_bin = binaries.c_lint()
        c_static_analyzer_bin = binaries.c_static_analyzer()
    except binaries.BinaryNotBuiltError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    cases = _cases(c_lint_bin, c_static_analyzer_bin)
    failures = []

    for fixture_name, expected_name, cmd in cases:
        result = run(cmd, cwd=FIXTURES_DIR, timeout=10)
        expected_path = FIXTURES_DIR / expected_name

        if args.update:
            expected_path.write_text(result.stdout)
            print(f"UPDATED {expected_name}")
            continue

        if not expected_path.exists():
            failures.append(f"{fixture_name}: no expected file at {expected_path} (run with --update to create it)")
            continue

        expected = expected_path.read_text()
        if result.stdout != expected:
            diff = "".join(
                difflib.unified_diff(
                    expected.splitlines(keepends=True),
                    result.stdout.splitlines(keepends=True),
                    fromfile=expected_name,
                    tofile="actual stdout",
                )
            )
            failures.append(f"{fixture_name}: stdout did not match {expected_name}\n{diff}")
        else:
            print(f"PASS  {fixture_name}")

    if args.update:
        return 0

    if failures:
        print("\n=== FAILURES ===\n")
        for failure in failures:
            print(failure)
            print()
        print(f"{len(failures)} failure(s)")
        return 1

    print(f"\nAll {len(cases)} negative-test cases passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
