#!/usr/bin/env python3
"""Renders testing/benchmarks/results.json (from run_benchmarks.py) as
two grouped-bar-chart PNGs: execution time and compile time, each a
1x2 (-O0 | -O2) small-multiple comparing minic against clang per
program.

A missing data point (this suite's own exploration found that
`minic sieve.mc -O2` hangs indefinitely at compile time -- see
programs/sieve.mc) is rendered as a zero-height hatched bar labeled
"N/A", not silently dropped -- the gap is part of the honest picture.

Requires matplotlib (see testing/requirements.txt); not a dependency of
the other testing/ suites.
"""

import argparse
import json
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

OPT_LEVELS = ["-O0", "-O2"]
TOOLCHAINS = ["minic", "clang"]

# Fixed categorical color assignment (never cycled/reassigned per chart) --
# slot 1 (blue) and slot 2 (aqua) from this repo's validated default
# palette (see the dataviz skill's references/palette.md).
COLORS = {"minic": "#2a78d6", "clang": "#1baf7a"}
NA_COLOR = "#c3c2b7"
TEXT_COLOR = "#0b0b0b"
MUTED_TEXT = "#52514e"
GRID_COLOR = "#e5e4df"


def _metric_ms(entry: dict, metric: str):
    """entry is results[program][opt][toolchain]; metric is "compile" or
    "run". Returns the mean in milliseconds, or None if that measurement
    failed (see run_benchmarks.py's failure handling)."""
    sub = entry.get(metric)
    if sub is None:
        return None
    return sub["mean"] * 1000


def _plot_metric(results: dict, metric: str, ylabel: str, title: str, out_path: Path):
    programs = sorted(results)

    fig, axes = plt.subplots(1, len(OPT_LEVELS), figsize=(11, 4.5), sharey=False)
    fig.suptitle(title, fontsize=13, color=TEXT_COLOR, fontweight="bold")

    bar_width = 0.32
    x = range(len(programs))

    for ax, opt in zip(axes, OPT_LEVELS):
        for i, toolchain in enumerate(TOOLCHAINS):
            offsets = [xi + (i - 0.5) * bar_width for xi in x]
            values = []
            missing = []
            for program in programs:
                entry = results[program].get(opt, {}).get(toolchain, {})
                ms = _metric_ms(entry, metric)
                if ms is None:
                    values.append(0)
                    missing.append(True)
                else:
                    values.append(ms)
                    missing.append(False)

            bars = ax.bar(
                offsets,
                values,
                width=bar_width,
                label=toolchain,
                color=[NA_COLOR if m else COLORS[toolchain] for m in missing],
                hatch=["///" if m else None for m in missing],
                edgecolor="white",
                linewidth=0.5,
            )
            for bar, is_missing in zip(bars, missing):
                if is_missing:
                    ax.text(
                        bar.get_x() + bar.get_width() / 2,
                        0.5,
                        "N/A",
                        ha="center",
                        va="bottom",
                        fontsize=7,
                        color=MUTED_TEXT,
                        rotation=90,
                    )

        ax.set_title(opt, fontsize=11, color=MUTED_TEXT)
        ax.set_xticks(list(x))
        ax.set_xticklabels(programs, fontsize=9, color=TEXT_COLOR)
        ax.set_ylabel(ylabel, fontsize=9, color=MUTED_TEXT)
        ax.grid(axis="y", color=GRID_COLOR, linewidth=0.8, zorder=0)
        ax.set_axisbelow(True)
        for spine in ("top", "right"):
            ax.spines[spine].set_visible(False)
        for spine in ("left", "bottom"):
            ax.spines[spine].set_color(GRID_COLOR)
        ax.tick_params(colors=MUTED_TEXT)

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", bbox_to_anchor=(0.99, 0.97), frameon=False, fontsize=9)

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"wrote {out_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("results_json", type=Path)
    parser.add_argument("--out-dir", type=Path, default=Path("testing/benchmarks/charts"))
    args = parser.parse_args()

    results = json.loads(args.results_json.read_text())
    if not results:
        print("error: results.json is empty", file=sys.stderr)
        return 2

    _plot_metric(
        results, "run", "run time (ms)", "Execution time: minic vs clang", args.out_dir / "execution_time.png"
    )
    _plot_metric(
        results, "compile", "compile time (ms)", "Compile time: minic vs clang", args.out_dir / "compile_time.png"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
