"""Subprocess run/capture helpers shared across the testing/ suites."""

import subprocess
from dataclasses import dataclass
from typing import Sequence


@dataclass
class RunResult:
    exit_code: int
    stdout: str
    stderr: str


def run(cmd: Sequence[str], cwd=None, timeout: float = 30) -> RunResult:
    try:
        proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        return RunResult(proc.returncode, proc.stdout, proc.stderr)
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout.decode() if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        stderr = exc.stderr.decode() if isinstance(exc.stderr, bytes) else (exc.stderr or "")
        return RunResult(-1, stdout, stderr + f"\n[timed out after {timeout}s]")
