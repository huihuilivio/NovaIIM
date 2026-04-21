#!/usr/bin/env python3
"""配置 NovaIIM CMake 构建。

用法:
    python scripts/configure.py                            # 默认 Release，启用 server + client
    python scripts/configure.py --build-type Debug --test  # Debug + 测试
    python scripts/configure.py --no-client                # 仅构建 server
    python scripts/configure.py --no-server --client       # 仅构建 client
"""
import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, run, which


def main() -> int:
    parser = argparse.ArgumentParser(description="Configure the NovaIIM CMake build.")
    parser.add_argument("--build-type", default="Release",
                        choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
                        help="CMake build type (default: Release).")
    parser.add_argument("--enable-coverage", action="store_true", help="Enable coverage support.")
    parser.add_argument("--test", action="store_true", help="Enable building tests (NOVA_BUILD_TESTS=ON).")
    parser.add_argument("--generator", default=None, help="CMake generator override.")
    parser.add_argument("--server", action="store_true", default=True, help="Build server (default: on).")
    parser.add_argument("--no-server", action="store_false", dest="server", help="Disable server build.")
    parser.add_argument("--client", action="store_true", default=True, help="Build client (default: on).")
    parser.add_argument("--no-client", action="store_false", dest="client", help="Disable client build.")
    parser.add_argument("--mysql", action="store_true", default=False, help="Enable MySQL backend.")
    args = parser.parse_args()

    if not which("cmake"):
        print("Error: cmake is not available in PATH.")
        return 1

    root = project_root()
    build_dir = root / "build"
    cmake_args = [
        "cmake", "-S", str(root), "-B", str(build_dir),
        f"-DCMAKE_BUILD_TYPE={args.build_type}",
        f"-DNOVA_BUILD_SERVER={'ON' if args.server else 'OFF'}",
        f"-DNOVA_BUILD_CLIENT={'ON' if args.client else 'OFF'}",
    ]

    if args.enable_coverage:
        cmake_args.append("-DENABLE_COVERAGE=ON")
    if args.test:
        cmake_args.append("-DNOVA_BUILD_TESTS=ON")
    if args.mysql:
        cmake_args.append("-DNOVA_ENABLE_MYSQL=ON")
    if args.generator:
        cmake_args.extend(["-G", args.generator])

    run(cmake_args, cwd=root)
    print("Configuration completed successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
