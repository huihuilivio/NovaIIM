#!/usr/bin/env python3
"""部署 NovaIIM 构建产物。

用法:
    python scripts/deploy.py                       # 部署全部
    python scripts/deploy.py server                # 仅部署服务端
    python scripts/deploy.py server admin-web      # 部署服务端 + Admin 前端
"""
import argparse
import shutil
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import (
    project_root, build_dir, output_bin, resolve_targets,
    CMAKE_TARGETS, WEB_PROJECTS, EXECUTABLES, ALL_TARGETS,
)


def copy_tree(source: Path, target: Path) -> None:
    if source.exists():
        shutil.copytree(source, target, dirs_exist_ok=True)
        print(f"Copied {source} → {target}")
    else:
        print(f"Warning: {source} not found, skipping")


def deploy_server(deploy_dir: Path) -> None:
    """Deploy server binary + configs."""
    copy_tree(output_bin(), deploy_dir / "bin")
    copy_tree(build_dir() / "output" / "lib", deploy_dir / "lib")
    configs_src = project_root() / "configs"
    if configs_src.exists():
        copy_tree(configs_src, deploy_dir / "configs")


def deploy_web(target: str, deploy_dir: Path) -> None:
    """Deploy pre-built web frontend dist."""
    web_dir = WEB_PROJECTS[target]
    dist = web_dir / "dist"
    if not dist.exists():
        print(f"Warning: {target} dist not found at {dist}. Run 'python scripts/build.py {target}' first.")
        return
    copy_tree(dist, deploy_dir / target)


def deploy_desktop(deploy_dir: Path) -> None:
    """Deploy desktop executable."""
    copy_tree(output_bin(), deploy_dir / "bin")


def main() -> int:
    parser = argparse.ArgumentParser(description="Deploy NovaIIM build artifacts.")
    parser.add_argument("targets", nargs="*", default=[],
                        help=f"Targets to deploy (default: all). Available: {', '.join(ALL_TARGETS)}")
    parser.add_argument("--deploy-path", default="deploy", help="Target deployment directory.")
    parser.add_argument("--clean", action="store_true", help="Clean deployment directory before copying.")
    args = parser.parse_args()

    targets = resolve_targets(args.targets or None)
    deploy_dir = project_root() / args.deploy_path

    if args.clean and deploy_dir.exists():
        shutil.rmtree(deploy_dir)
    deploy_dir.mkdir(parents=True, exist_ok=True)

    for t in targets:
        print(f"\n--- Deploying: {t} ---")
        if t == "server":
            deploy_server(deploy_dir)
        elif t == "desktop":
            deploy_desktop(deploy_dir)
        elif t == "sdk":
            copy_tree(build_dir() / "output" / "lib", deploy_dir / "lib")
        elif t in WEB_PROJECTS:
            deploy_web(t, deploy_dir)

    print(f"\nDeployment completed → {deploy_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
