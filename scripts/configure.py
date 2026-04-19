#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, run, which


def main() -> int:
    parser = argparse.ArgumentParser(description="Configure the NovaIIM CMake build.")
    parser.add_argument("--build-type", default="Release", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], help="CMake build type.")
    parser.add_argument("--enable-coverage", action="store_true", help="Enable coverage support during configure.")
    parser.add_argument("--test", action="store_true", help="Enable building tests (NOVA_BUILD_TESTS=ON).")
    parser.add_argument("--generator", default=None, help="Optional CMake generator.")
    args = parser.parse_args()

    if not which("cmake"):
        print("Error: cmake is not available in PATH.")
        return 1

    root = project_root()
    build_dir = root / "build"
    cmake_args = ["cmake", "-S", str(root), "-B", str(build_dir), f"-DCMAKE_BUILD_TYPE={args.build_type}"]

    if args.enable_coverage:
        cmake_args.append("-DENABLE_COVERAGE=ON")
    if args.test:
        cmake_args.append("-DNOVA_BUILD_TESTS=ON")
    if args.generator:
        cmake_args.extend(["-G", args.generator])

    run(cmake_args, cwd=root)
    print("Configuration completed successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
