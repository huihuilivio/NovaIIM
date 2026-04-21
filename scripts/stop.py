#!/usr/bin/env python3
"""停止 NovaIIM 子项目。

用法:
    python scripts/stop.py              # 停止服务端
    python scripts/stop.py server       # 停止服务端
    python scripts/stop.py desktop      # 停止桌面客户端
    python scripts/stop.py --force      # 强制搜索并停止
    python scripts/stop.py --all        # 停止所有已知进程
"""
from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, EXECUTABLES


def is_running(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except (OSError, SystemError):
        return False
    return True


def stop_process(pid: int) -> None:
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(pid), "/F"], check=False)
    else:
        try:
            os.kill(pid, signal.SIGTERM)
        except OSError:
            pass


def find_process_by_name(exe_name: str) -> int | None:
    if os.name == "nt":
        try:
            output = subprocess.check_output(["tasklist"], text=True)
            for line in output.splitlines():
                if exe_name in line:
                    parts = line.split()
                    if len(parts) >= 2 and parts[1].isdigit():
                        return int(parts[1])
        except subprocess.CalledProcessError:
            return None
    else:
        base = exe_name.replace(".exe", "")
        try:
            output = subprocess.check_output(["pgrep", "-f", base], text=True)
            for line in output.splitlines():
                if line.strip().isdigit():
                    return int(line.strip())
        except subprocess.CalledProcessError:
            return None
    return None


def pid_file(target: str) -> Path:
    return project_root() / f"{target}.pid"


def stop_target(target: str, force: bool) -> int:
    pf = pid_file(target)

    if pf.exists():
        try:
            pid = int(pf.read_text().strip())
            if is_running(pid):
                stop_process(pid)
                print(f"{target} stopped (PID: {pid})")
            else:
                print(f"{target}: no running process at PID {pid}")
            pf.unlink()
            return 0
        except ValueError:
            pf.unlink()
            print(f"{target}: invalid PID file removed")

    if force:
        exe_name = EXECUTABLES.get(target)
        if exe_name:
            pid = find_process_by_name(exe_name)
            if pid:
                stop_process(pid)
                print(f"{target} stopped (PID: {pid})")
                return 0
        print(f"{target}: no running process found")
        return 1

    print(f"{target}: PID file not found. Use --force to scan for running processes.")
    return 1


def main() -> int:
    stoppable = sorted(EXECUTABLES.keys())
    parser = argparse.ArgumentParser(description="Stop NovaIIM subprojects.")
    parser.add_argument("targets", nargs="*", default=["server"],
                        help=f"Targets to stop (default: server). Available: {', '.join(stoppable)}")
    parser.add_argument("--force", action="store_true", help="Search for running processes if PID file is missing.")
    parser.add_argument("--all", action="store_true", help="Stop all known executable targets.")
    args = parser.parse_args()

    targets = stoppable if args.all else args.targets

    unknown = set(targets) - set(stoppable)
    if unknown:
        print(f"Error: cannot stop: {', '.join(sorted(unknown))} (only executables can be stopped)")
        print(f"Stoppable: {', '.join(stoppable)}")
        return 1

    rc = 0
    for t in targets:
        rc |= stop_target(t, args.force)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
