import os
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent


def project_root() -> Path:
    return PROJECT_ROOT


def root_path(*segments) -> Path:
    return PROJECT_ROOT.joinpath(*segments)


def which(executable: str) -> str | None:
    return shutil.which(executable)


def run(command, cwd=None, env=None, check=True):
    if isinstance(command, (list, tuple)):
        cmd = list(command)
    else:
        cmd = command

    cmd_str = command if isinstance(command, str) else " ".join(map(str, cmd))
    print(f"Running: {cmd_str}")

    result = subprocess.run(cmd, cwd=cwd, env={**os.environ, **(env or {})}, shell=False)
    if check and result.returncode != 0:
        raise SystemExit(result.returncode)
    return result
