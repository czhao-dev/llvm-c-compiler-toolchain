#!/usr/bin/env python3
"""Renders testing/benchmarks/results.json (from run_benchmarks.py) as a
Markdown comparison table."""

import argparse
import json
import sys
from pathlib import Path


def _fmt(hyperfine_result, field: str) -> str:
    if hyperfine_result is None:
        return "FAILED"
    return f"{hyperfine_result[field] * 1000:.1f}"


def render_markdown(results: dict) -> str:
    lines = [
        "| Program | Opt | Toolchain | Compile (ms) | Run mean (ms) | Run median (ms) | Run stddev (ms) |",
        "|---|---|---|---|---|---|---|",
    ]
    for program in sorted(results):
        for opt in sorted(results[program]):
            for toolchain in ("minic", "clang"):
                entry = results[program][opt].get(toolchain, {})
                compile_result = entry.get("compile")
                run_result = entry.get("run")
                lines.append(
                    "| {program} | {opt} | {toolchain} | {compile} | {mean} | {median} | {stddev} |".format(
                        program=program,
                        opt=opt,
                        toolchain=toolchain,
                        compile=_fmt(compile_result, "mean"),
                        mean=_fmt(run_result, "mean"),
                        median=_fmt(run_result, "median"),
                        stddev=_fmt(run_result, "stddev"),
                    )
                )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results_json", type=Path)
    args = parser.parse_args()

    results = json.loads(args.results_json.read_text())
    if not results:
        print("error: results.json is empty", file=sys.stderr)
        return 2

    print(render_markdown(results), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
