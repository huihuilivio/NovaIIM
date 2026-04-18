#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, run


def main() -> int:
    parser = argparse.ArgumentParser(description="Build the NovaIIM project using CMake.")
    parser.add_argument("--config", default="Release", help="Build configuration.")
    args = parser.parse_args()

    root = project_root()
    build_dir = root / "build"

    if not build_dir.exists():
        print("Build directory not found. Please run configure.py first.")
        return 1

    run(["cmake", "--build", str(build_dir), "--config", args.config], cwd=root)
    print("Build completed successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
