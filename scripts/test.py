#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, run, which


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the NovaIIM test suite.")
    parser.add_argument("--config", default=None, help="Optional build configuration for multi-config generators.")
    args = parser.parse_args()

    if not which("ctest"):
        print("Error: ctest is not available in PATH.")
        return 1

    root = project_root()
    build_dir = root / "build"
    if not build_dir.exists():
        print("Build directory not found. Please run configure.py and build.py first.")
        return 1

    cmd = ["ctest", "--output-on-failure"]
    if args.config:
        cmd.extend(["-C", args.config])

    run(cmd, cwd=build_dir)
    print("All tests completed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
