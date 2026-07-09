#!/usr/bin/env python3
"""Benchmark harness: toolchain compile-speed and execution-speed
comparison between minic and clang.

For each program in testing/benchmarks/programs/*.mc, at -O0 and -O2,
measures (via hyperfine):
  - compile time: the compiler invocation itself
  - run time: the resulting binary, executed with no arguments

for both `minic` and `clang -x c`. hyperfine is a hard dependency for
this suite specifically (unlike the differential/negative suites) --
`brew install hyperfine` locally; installed explicitly in CI.

A compile or run that fails or hangs is recorded as a failure for that
(program, toolchain, opt) combination rather than aborting the whole
run -- this suite's own exploration surfaced a real pre-existing minic
bug where `sieve.mc -O2` hangs indefinitely at compile time (see that
file's header comment), which this harness needs to survive, not paper
over.

Writes raw results as JSON. See report.py for a Markdown table and
plot.py for charts generated from that JSON.
"""

import argparse
import json
import shlex
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import binaries  # noqa: E402
from common.proc import run  # noqa: E402

PROGRAMS_DIR = Path(__file__).resolve().parent / "programs"
OPT_LEVELS = ["-O0", "-O2"]

COMPILE_WARMUP = 1
COMPILE_RUNS = 3
COMPILE_TIMEOUT = 60  # wall-clock budget for the whole hyperfine compile invocation

RUN_WARMUP = 3
RUN_RUNS = 10
RUN_TIMEOUT = 60


def find_hyperfine() -> str:
    hf = shutil.which("hyperfine")
    if hf is None:
        print(
            "error: hyperfine not found on PATH -- install it first "
            "(brew install hyperfine, or apt-get install hyperfine)",
            file=sys.stderr,
        )
        sys.exit(2)
    return hf


def hyperfine_benchmark(hf_bin: str, cmd: list, warmup: int, runs: int, timeout: float):
    """Runs `cmd` (an argv list) under hyperfine. Returns the parsed
    per-command result dict (hyperfine's native seconds-based fields) or
    None if the command failed, or if hyperfine itself didn't finish
    within `timeout` (hyperfine has no internal per-run timeout, so a
    hanging benchmarked command -- see sieve.mc's -O2 issue -- is only
    bounded by killing the whole hyperfine process from the outside)."""
    with tempfile.TemporaryDirectory(prefix="hyperfine_") as tmp:
        json_path = Path(tmp) / "result.json"
        hf_cmd = [
            hf_bin,
            "--warmup", str(warmup),
            "--runs", str(runs),
            "--export-json", str(json_path),
            "--",
            shlex.join(cmd),
        ]
        result = run(hf_cmd, timeout=timeout)
        if result.exit_code != 0 or not json_path.exists():
            return None
        data = json.loads(json_path.read_text())
        return data["results"][0]


def minic_compile_cmd(minic_bin: Path, mc_path: Path, opt: str, out_path: Path) -> list:
    return [str(minic_bin), str(mc_path), opt, "-o", str(out_path)]


def clang_compile_cmd(clang_bin: str, mc_path: Path, opt: str, out_path: Path) -> list:
    # -std=c99 -Wno-implicit-function-declaration: MiniC examples call
    # printf() with no #include <stdio.h>, see
    # testing/differential/run_differential_tests.py for the same flags.
    return [
        clang_bin, "-x", "c", "-std=c99", "-Wno-implicit-function-declaration",
        opt, str(mc_path), "-o", str(out_path),
    ]


def benchmark_one(hf_bin: str, toolchain: str, compile_cmd: list, out_path: Path) -> dict:
    """Benchmarks one (program, toolchain, opt) combination. Returns a
    dict with "compile" and "run" keys, each either a hyperfine result
    dict or None on failure."""
    compile_result = hyperfine_benchmark(hf_bin, compile_cmd, COMPILE_WARMUP, COMPILE_RUNS, COMPILE_TIMEOUT)
    if compile_result is None:
        print(f"  {toolchain}: compile FAILED or timed out")
        return {"compile": None, "run": None}

    if not out_path.is_file():
        print(f"  {toolchain}: compile reported success but no binary was produced")
        return {"compile": compile_result, "run": None}

    run_result = hyperfine_benchmark(hf_bin, [str(out_path)], RUN_WARMUP, RUN_RUNS, RUN_TIMEOUT)
    if run_result is None:
        print(f"  {toolchain}: run FAILED or timed out")
        return {"compile": compile_result, "run": None}

    print(
        f"  {toolchain}: compile mean={compile_result['mean'] * 1000:.1f}ms "
        f"run mean={run_result['mean'] * 1000:.1f}ms"
    )
    return {"compile": compile_result, "run": run_result}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", type=Path, default=Path("testing/benchmarks/results.json"))
    args = parser.parse_args()

    hf_bin = find_hyperfine()
    try:
        minic_bin = binaries.minic()
    except binaries.BinaryNotBuiltError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    clang_bin = shutil.which("clang")
    if clang_bin is None:
        print("error: clang not found on PATH", file=sys.stderr)
        return 2

    programs = sorted(PROGRAMS_DIR.glob("*.mc"))
    if not programs:
        print(f"error: no .mc programs found in {PROGRAMS_DIR}", file=sys.stderr)
        return 2

    results = {}
    with tempfile.TemporaryDirectory(prefix="minic_benchmarks_") as tmp:
        tmpdir = Path(tmp)
        for program_path in programs:
            program_name = program_path.stem
            results[program_name] = {}
            for opt in OPT_LEVELS:
                print(f"{program_name} {opt}")
                results[program_name][opt] = {}

                minic_out = tmpdir / f"{program_name}_minic{opt}"
                results[program_name][opt]["minic"] = benchmark_one(
                    hf_bin, "minic", minic_compile_cmd(minic_bin, program_path, opt, minic_out), minic_out
                )

                clang_out = tmpdir / f"{program_name}_clang{opt}"
                results[program_name][opt]["clang"] = benchmark_one(
                    hf_bin, "clang", clang_compile_cmd(clang_bin, program_path, opt, clang_out), clang_out
                )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, indent=2))
    print(f"\nWrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
