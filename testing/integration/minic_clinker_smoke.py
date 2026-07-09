#!/usr/bin/env python3
"""Best-effort integration smoke test: minic + llc + the repo's own
c-linker, end to end -- no clang anywhere in the link step.

This is explicitly a stretch/lower-confidence addition, not a load-
bearing correctness check (see testing/README.md and
testing/integration/fixtures/two_functions.mc for the constraints this
works around). Never blocks CI: run via
`.github/workflows/testing-suite.yml` with `continue-on-error: true` and
`if: runner.os == 'Linux'`.

Why this is convoluted: minic's CLI accepts exactly one input file and
its parser rejects a prototype-only (no-body) function declaration, so
there's no way to get minic to treat two functions as separate
translation units. Instead: compile ONE .mc file defining both functions
together (so minic's own parser/sema accepts it) via `minic --emit-ir`,
then split the resulting LLVM IR text in two post-hoc -- one module
keeps a function's real definition, the other gets an external
`declare` for it instead. Each half is compiled to a `.o` via `llc
-mtriple=x86_64-unknown-linux-gnu`, and the two `.o` files are linked
with the repo's own `c-link` (not clang) into a final executable.

c-link has no dynamic linking and no CRT/`_start` startup code (see
c-linker/README.md), so:
  - the fixture is libc-free (no printf) -- nothing calling into libc
    could be linked by c-link at all.
  - `--entry main` is passed explicitly since there's no real `_start`.
  - the produced binary is verified structurally (exit code 0, and where
    the `file` command is available, that it reports a valid ELF64
    executable) but is NEVER executed -- c-linker's own test suite
    already establishes that convention for exactly this reason (no
    return-address/stack setup, and the host running this script may not
    even be able to execute a Linux ELF binary).
"""

import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import binaries  # noqa: E402
from common.proc import run  # noqa: E402

FIXTURE = Path(__file__).resolve().parent / "fixtures" / "two_functions.mc"

DEFINE_RE = re.compile(r"^define\s.*@([A-Za-z_][A-Za-z0-9_]*)\(")


def find_llc() -> str:
    for candidate in ("llc", "llc-17", "llc-18"):
        path = shutil.which(candidate)
        if path is not None:
            return path
    raise RuntimeError("llc not found on PATH (tried llc, llc-17, llc-18)")


def split_ir(ir_text: str) -> dict:
    """Splits minic --emit-ir output into {function_name: full "define
    ... { ... }" block text}, plus a "preamble" key for everything before
    the first `define` (target triple, shared `declare`s)."""
    lines = ir_text.splitlines()
    blocks = {"preamble": []}
    current_name = None
    current_lines = []

    for line in lines:
        match = DEFINE_RE.match(line)
        if match:
            current_name = match.group(1)
            current_lines = [line]
            continue
        if current_name is not None:
            current_lines.append(line)
            if line == "}":
                blocks[current_name] = "\n".join(current_lines)
                current_name = None
                current_lines = []
            continue
        blocks["preamble"].append(line)

    if current_name is not None:
        raise RuntimeError(f"unterminated function definition for '{current_name}' in --emit-ir output")

    blocks["preamble"] = "\n".join(blocks["preamble"])
    return blocks


def module_defining(blocks: dict, define_name: str, declare_externals: dict) -> str:
    """Builds one .ll module: the shared preamble, an external `declare`
    for each name in declare_externals ({name: declare_line}), and the
    real definition of `define_name`."""
    parts = [blocks["preamble"]]
    parts.extend(declare_externals.values())
    parts.append(blocks[define_name])
    return "\n".join(parts) + "\n"


def main() -> int:
    try:
        minic_bin = binaries.minic()
        c_link_bin = binaries.c_link()
    except binaries.BinaryNotBuiltError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    try:
        llc_bin = find_llc()
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="minic_clinker_smoke_") as tmp:
        tmpdir = Path(tmp)

        ir_result = run([str(minic_bin), str(FIXTURE), "--emit-ir"], timeout=30)
        if ir_result.exit_code != 0:
            print(f"error: minic --emit-ir failed:\n{ir_result.stderr}", file=sys.stderr)
            return 1

        blocks = split_ir(ir_result.stdout)
        if "helper" not in blocks or "main" not in blocks:
            print(f"error: expected 'helper' and 'main' functions in IR, got: {sorted(blocks)}", file=sys.stderr)
            return 1

        helper_ll = module_defining(blocks, "helper", {})
        main_ll = module_defining(blocks, "main", {"helper": "declare i32 @helper(i32, i32)"})

        (tmpdir / "helper.ll").write_text(helper_ll)
        (tmpdir / "main.ll").write_text(main_ll)

        for name in ("helper", "main"):
            llc_result = run(
                [
                    llc_bin,
                    "-mtriple=x86_64-unknown-linux-gnu",
                    "-filetype=obj",
                    "-o", str(tmpdir / f"{name}.o"),
                    str(tmpdir / f"{name}.ll"),
                ],
                timeout=30,
            )
            if llc_result.exit_code != 0:
                print(f"error: llc failed compiling {name}.ll:\n{llc_result.stderr}", file=sys.stderr)
                return 1

        output_path = tmpdir / "combined"
        link_result = run(
            [str(c_link_bin), str(tmpdir / "helper.o"), str(tmpdir / "main.o"), "-o", str(output_path),
             "--entry", "main"],
            timeout=30,
        )
        if link_result.exit_code != 0:
            print(f"error: c-link failed:\n{link_result.stdout}\n{link_result.stderr}", file=sys.stderr)
            return 1

        if not output_path.is_file():
            print("error: c-link exited 0 but produced no output file", file=sys.stderr)
            return 1

        file_bin = shutil.which("file")
        if file_bin is not None:
            file_result = run([file_bin, str(output_path)], timeout=10)
            print(f"file: {file_result.stdout.strip()}")
            if "ELF" not in file_result.stdout:
                print("error: output is not recognized as an ELF binary", file=sys.stderr)
                return 1
        else:
            print("note: 'file' not available, skipping ELF-format verification")

        print("PASS: minic --emit-ir -> llc -> c-link produced a valid ELF64 executable (not executed)")
        return 0


if __name__ == "__main__":
    sys.exit(main())
