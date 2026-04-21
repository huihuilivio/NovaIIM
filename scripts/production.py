#!/usr/bin/env python3
"""一键生产构建 NovaIIM — configure + build + deploy。

用法:
    python scripts/production.py                    # 全部子项目
    python scripts/production.py server             # 仅服务端
    python scripts/production.py server admin-web   # 服务端 + Admin 前端
"""
import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import (
    project_root, run, build_dir, ensure_npm, resolve_targets,
    CMAKE_TARGETS, WEB_PROJECTS, ALL_TARGETS,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Production build for NovaIIM.")
    parser.add_argument("targets", nargs="*", default=[],
                        help=f"Targets to build (default: all). Available: {', '.join(ALL_TARGETS)}")
    parser.add_argument("--config", default="Release", help="Build configuration (default: Release).")
    args = parser.parse_args()

    root = project_root()
    bd = build_dir()
    targets = resolve_targets(args.targets or None)

    # Configure if needed
    cmake_targets = [t for t in targets if t in CMAKE_TARGETS]
    if cmake_targets:
        if not bd.exists() or not (bd / "CMakeCache.txt").exists():
            run(["cmake", "-S", str(root), "-B", str(bd),
                 f"-DCMAKE_BUILD_TYPE={args.config}"], cwd=root)
        for t in cmake_targets:
            cmake_name = CMAKE_TARGETS[t]
            run(["cmake", "--build", str(bd), "--config", args.config, "--target", cmake_name],
                cwd=root)

    # Build web targets
    web_targets = [t for t in targets if t in WEB_PROJECTS]
    for t in web_targets:
        web_dir = WEB_PROJECTS[t]
        if not web_dir.exists():
            print(f"Warning: {web_dir} not found, skipping {t}")
            continue
        ensure_npm(web_dir)
        run(["npx", "vue-tsc", "--noEmit"], cwd=web_dir)
        run(["npx", "vite", "build"], cwd=web_dir)

    print(f"\nProduction build completed: {', '.join(targets)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
