#!/usr/bin/env python3
"""构建 NovaIIM 子项目。

用法:
    python scripts/build.py                    # 构建全部
    python scripts/build.py server             # 仅构建服务端
    python scripts/build.py desktop admin-web  # 构建桌面客户端 + Admin 前端
    python scripts/build.py --list             # 列出所有可用目标
"""
import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import (
    project_root, run, build_dir, ensure_npm,
    resolve_targets, CMAKE_TARGETS, WEB_PROJECTS, ALL_TARGETS,
)


def build_cmake_target(target: str, config: str) -> None:
    bd = build_dir()
    if not bd.exists():
        print("Build directory not found. Please run configure.py first.")
        raise SystemExit(1)
    cmake_name = CMAKE_TARGETS[target]
    run(["cmake", "--build", str(bd), "--config", config, "--target", cmake_name],
        cwd=project_root())


def build_web_target(target: str) -> None:
    web_dir = WEB_PROJECTS[target]
    if not web_dir.exists():
        print(f"Warning: {web_dir} not found, skipping {target}")
        return
    ensure_npm(web_dir)
    run(["npx", "vue-tsc", "--noEmit"], cwd=web_dir)
    run(["npx", "vite", "build"], cwd=web_dir)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build NovaIIM subprojects.")
    parser.add_argument("targets", nargs="*", default=[], help=f"Targets to build (default: all). Available: {', '.join(ALL_TARGETS)}")
    parser.add_argument("--config", default="Release", help="CMake build configuration (default: Release).")
    parser.add_argument("--list", action="store_true", help="List available targets and exit.")
    args = parser.parse_args()

    if args.list:
        print("C++ targets (CMake):")
        for name, cmake_name in sorted(CMAKE_TARGETS.items()):
            print(f"  {name:15s} → {cmake_name}")
        print("Web targets (npm/vite):")
        for name, path in sorted(WEB_PROJECTS.items()):
            print(f"  {name:15s} → {path.relative_to(project_root())}")
        return 0

    targets = resolve_targets(args.targets or None)

    for t in targets:
        print(f"\n{'='*60}")
        print(f"  Building: {t}")
        print(f"{'='*60}")
        if t in CMAKE_TARGETS:
            build_cmake_target(t, args.config)
        elif t in WEB_PROJECTS:
            build_web_target(t)

    print(f"\nBuild completed: {', '.join(targets)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
