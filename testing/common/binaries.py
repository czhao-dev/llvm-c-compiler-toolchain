"""Locates each subproject's built binaries.

None of these helpers build anything themselves -- build orchestration
stays in each subproject's own scripts/configure.sh + CMake (and in CI,
see .github/workflows/testing-suite.yml). A missing binary is a clear,
actionable error rather than a silent attempt to build it.
"""

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


class BinaryNotBuiltError(RuntimeError):
    pass


def _binary(subproject: str, relative_path: str) -> Path:
    path = REPO_ROOT / subproject / relative_path
    if not path.is_file():
        raise BinaryNotBuiltError(
            f"{path} does not exist -- build it first:\n"
            f"  cd {subproject} && ./scripts/configure.sh && cmake --build build"
        )
    return path


def minic() -> Path:
    return _binary("c-compiler", "build/minic")


def c_link() -> Path:
    return _binary("c-linker", "build/c-link")
