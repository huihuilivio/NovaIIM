#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, run


def main() -> int:
    parser = argparse.ArgumentParser(description="Build NovaIIM for production.")
    parser.add_argument("--config", default="Release", help="Build configuration.")
    args = parser.parse_args()

    root = project_root()
    build_dir = root / "build"

    if not build_dir.exists() or not (build_dir / "CMakeCache.txt").exists():
        run(["cmake", "-S", str(root), "-B", str(build_dir), f"-DCMAKE_BUILD_TYPE={args.config}"], cwd=root)

    run(["cmake", "--build", str(build_dir), "--config", args.config], cwd=root)
    print("Production build completed successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
