#!/usr/bin/env python3
import argparse
import shutil
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, run


def copy_tree(source: Path, target: Path) -> None:
    if source.exists():
        shutil.copytree(source, target, dirs_exist_ok=True)
        print(f"Copied {source} to {target}")
    else:
        print(f"Warning: source path not found: {source}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Deploy NovaIIM build artifacts.")
    parser.add_argument("--deploy-path", default="deploy", help="Target deployment path.")
    parser.add_argument("--config", default="Release", help="Build configuration.")
    parser.add_argument("--clean", action="store_true", help="Clean the deployment directory before copying.")
    args = parser.parse_args()

    root = project_root()
    build_dir = root / "build"
    if not build_dir.exists():
        print("Build directory not found. Please build first.")
        return 1

    target_dir = root / args.deploy_path
    if args.clean and target_dir.exists():
        shutil.rmtree(target_dir)

    target_dir.mkdir(parents=True, exist_ok=True)

    copy_tree(build_dir / "output" / "bin", target_dir / "bin")
    copy_tree(build_dir / "output" / "lib", target_dir / "lib")

    print(f"Deployment completed to {target_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
