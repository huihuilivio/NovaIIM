from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent

_msvc_env_cache: dict[str, str] | None = None


def project_root() -> Path:
    return PROJECT_ROOT


def root_path(*segments) -> Path:
    return PROJECT_ROOT.joinpath(*segments)


def which(executable: str) -> str | None:
    return shutil.which(executable)


def _find_vcvarsall() -> Path | None:
    """Locate vcvarsall.bat via vswhere or common paths."""
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.exists():
        try:
            result = subprocess.run(
                [str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                capture_output=True, text=True, check=True,
            )
            vs_path = Path(result.stdout.strip())
            vcvars = vs_path / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
            if vcvars.exists():
                return vcvars
        except (subprocess.CalledProcessError, OSError):
            pass

    # Fallback: scan common VS install locations
    for edition in ("Professional", "Enterprise", "Community", "BuildTools"):
        for year in ("2022", "2019"):
            for pf in (os.environ.get("ProgramFiles", ""), os.environ.get("ProgramFiles(x86)", "")):
                if not pf:
                    continue
                vcvars = Path(pf) / "Microsoft Visual Studio" / year / edition / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
                if vcvars.exists():
                    return vcvars
    return None


def get_msvc_env() -> dict[str, str]:
    """Capture the MSVC developer environment variables on Windows."""
    global _msvc_env_cache
    if _msvc_env_cache is not None:
        return _msvc_env_cache

    # If UCRT headers are already available, the environment is fine as-is
    include = os.environ.get("INCLUDE", "")
    if "ucrt" in include.lower():
        _msvc_env_cache = dict(os.environ)
        return _msvc_env_cache

    vcvars = _find_vcvarsall()
    if vcvars is None:
        print("Warning: Could not find vcvarsall.bat. Build may fail if MSVC environment is not set.")
        _msvc_env_cache = dict(os.environ)
        return _msvc_env_cache

    # Run vcvarsall and capture the resulting environment
    try:
        result = subprocess.run(
            f'"{vcvars}" x64 >nul 2>&1 && set',
            capture_output=True, text=True, shell=True, check=True,
        )
        env = {}
        for line in result.stdout.splitlines():
            if "=" in line:
                key, _, value = line.partition("=")
                env[key] = value
        _msvc_env_cache = env
        return _msvc_env_cache
    except (subprocess.CalledProcessError, OSError) as e:
        print(f"Warning: Failed to activate MSVC environment: {e}")
        _msvc_env_cache = dict(os.environ)
        return _msvc_env_cache


def ensure_build_env() -> dict[str, str]:
    """Return an environment dict suitable for building on any platform."""
    if os.name == "nt":
        return get_msvc_env()
    return dict(os.environ)


def run(command, cwd=None, env=None, check=True):
    if isinstance(command, (list, tuple)):
        cmd = list(command)
        cmd_str = " ".join(map(str, cmd))
        use_shell = False
    else:
        cmd = command
        cmd_str = command
        use_shell = True

    print(f"Running: {cmd_str}")

    base_env = env if env is not None else ensure_build_env()
    result = subprocess.run(cmd, cwd=cwd, env=base_env, shell=use_shell)
    if check and result.returncode != 0:
        raise SystemExit(result.returncode)
    return result
