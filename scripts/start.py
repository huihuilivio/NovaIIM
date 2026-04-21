#!/usr/bin/env python3
"""启动 NovaIIM 子项目。

用法:
    python scripts/start.py                     # 启动服务端
    python scripts/start.py server              # 启动服务端
    python scripts/start.py admin-web           # 启动 Admin Web 开发服务器
    python scripts/start.py desktop-web         # 启动桌面前端开发服务器
    python scripts/start.py server admin-web    # 同时启动服务端 + Admin Web
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import (
    project_root, run, output_bin, ensure_npm,
    EXECUTABLES, WEB_PROJECTS, ALL_TARGETS,
)

STARTABLE = sorted(set(EXECUTABLES.keys()) | set(WEB_PROJECTS.keys()))


def is_running(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    return True


def pid_file(target: str) -> Path:
    return project_root() / f"{target}.pid"


def start_exe(target: str, config_file: str | None, log_file: str | None) -> int:
    root = project_root()
    pf = pid_file(target)

    if pf.exists():
        try:
            pid = int(pf.read_text().strip())
            if is_running(pid):
                print(f"{target} is already running (PID: {pid})")
                return 0
            pf.unlink()
        except ValueError:
            pf.unlink()

    exe = output_bin() / EXECUTABLES[target]
    if not exe.exists():
        print(f"Executable not found: {exe}")
        print(f"Please build {target} first: python scripts/build.py {target}")
        return 1

    cmd: list[str] = [str(exe)]
    if target == "server":
        cfg = root / (config_file or "configs/server.yaml")
        if not cfg.exists():
            print(f"Config file not found: {cfg}")
            return 1
        cmd.extend(["--config", str(cfg)])

    log_path = root / (log_file or f"logs/{target}.log")
    log_path.parent.mkdir(parents=True, exist_ok=True)

    popen_kwargs: dict = dict(stderr=subprocess.STDOUT, cwd=str(root))
    if os.name == "nt":
        popen_kwargs["creationflags"] = (
            subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
        )
    else:
        popen_kwargs["start_new_session"] = True

    with open(log_path, "ab") as lf:
        process = subprocess.Popen(cmd, stdout=lf, **popen_kwargs)

    pf.write_text(str(process.pid), encoding="utf-8")
    print(f"{target} started (PID: {process.pid}), logs: {log_path}")
    return 0


def start_web(target: str) -> int:
    web_dir = WEB_PROJECTS[target]
    if not web_dir.exists():
        print(f"Web directory not found: {web_dir}")
        return 1
    ensure_npm(web_dir)
    print(f"Starting {target} dev server (Ctrl+C to stop)...")
    run(["npx", "vite", "--open"], cwd=web_dir, check=False)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Start NovaIIM subprojects.")
    parser.add_argument("targets", nargs="*", default=["server"],
                        help=f"Targets to start (default: server). Available: {', '.join(STARTABLE)}")
    parser.add_argument("--config-file", default=None, help="Server config file (default: configs/server.yaml).")
    parser.add_argument("--log-file", default=None, help="Log file path (default: logs/<target>.log).")
    args = parser.parse_args()

    unknown = set(args.targets) - set(STARTABLE)
    if unknown:
        print(f"Error: cannot start: {', '.join(sorted(unknown))}")
        print(f"Startable targets: {', '.join(STARTABLE)}")
        return 1

    rc = 0
    for t in args.targets:
        if t in EXECUTABLES:
            rc |= start_exe(t, args.config_file, args.log_file)
        elif t in WEB_PROJECTS:
            rc |= start_web(t)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
