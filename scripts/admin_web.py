#!/usr/bin/env python3
"""server/web 管理后台前端脚本 — 统一管理 dev / build / test / lint 命令"""

import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, run, ensure_npm, WEB_PROJECTS


ADMIN_WEB_DIR = WEB_PROJECTS["admin-web"]


def ensure_node_modules():
    ensure_npm(ADMIN_WEB_DIR)


def cmd_install(_args):
    run(["npm", "install"], cwd=ADMIN_WEB_DIR)


def cmd_dev(_args):
    ensure_node_modules()
    run(["npx", "vite", "--open"], cwd=ADMIN_WEB_DIR, check=False)


def cmd_build(_args):
    ensure_node_modules()
    run(["npx", "vue-tsc", "--noEmit"], cwd=ADMIN_WEB_DIR)
    run(["npx", "vite", "build"], cwd=ADMIN_WEB_DIR)


def cmd_test(args):
    ensure_node_modules()
    cmd = ["npx", "vitest", "run"]
    if args.watch:
        cmd = ["npx", "vitest"]
    if args.coverage:
        cmd.append("--coverage")
    run(cmd, cwd=ADMIN_WEB_DIR, check=False)


def cmd_typecheck(_args):
    ensure_node_modules()
    run(["npx", "vue-tsc", "--noEmit"], cwd=ADMIN_WEB_DIR)


def cmd_preview(_args):
    ensure_node_modules()
    run(["npx", "vite", "preview"], cwd=ADMIN_WEB_DIR, check=False)


def main() -> int:
    parser = argparse.ArgumentParser(description="NovaIIM Admin Web 前端开发脚本")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("install", help="安装依赖 (npm install)")
    sub.add_parser("dev", help="启动开发服务器 (vite dev)")
    sub.add_parser("build", help="类型检查 + 生产构建")
    sub.add_parser("preview", help="预览生产构建")
    sub.add_parser("typecheck", help="仅运行 vue-tsc 类型检查")

    test_p = sub.add_parser("test", help="运行单元测试 (vitest)")
    test_p.add_argument("--watch", action="store_true", help="监听模式")
    test_p.add_argument("--coverage", action="store_true", help="收集覆盖率")

    args = parser.parse_args()
    handlers = {
        "install": cmd_install,
        "dev": cmd_dev,
        "build": cmd_build,
        "test": cmd_test,
        "typecheck": cmd_typecheck,
        "preview": cmd_preview,
    }
    handlers[args.command](args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
