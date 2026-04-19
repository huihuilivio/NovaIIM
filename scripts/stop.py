#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root


def is_running(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except OSError:
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


def find_server_process() -> int | None:
    if os.name == "nt":
        try:
            output = subprocess.check_output(["tasklist"], text=True)
            for line in output.splitlines():
                if "im_server.exe" in line:
                    parts = line.split()
                    if len(parts) >= 2 and parts[1].isdigit():
                        return int(parts[1])
        except subprocess.CalledProcessError:
            return None
    else:
        try:
            output = subprocess.check_output(["pgrep", "-f", "im_server"], text=True)
            for line in output.splitlines():
                if line.isdigit():
                    return int(line)
        except subprocess.CalledProcessError:
            return None
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Stop the NovaIIM server.")
    parser.add_argument("--force", action="store_true", help="Force stop the server if a PID file is not present.")
    args = parser.parse_args()

    root = project_root()
    pid_path = root / "server.pid"

    if pid_path.exists():
        try:
            pid = int(pid_path.read_text().strip())
            if is_running(pid):
                stop_process(pid)
                print(f"Server stopped (PID: {pid})")
            else:
                print(f"No running process found for PID {pid}.")
            pid_path.unlink()
            return 0
        except ValueError:
            pid_path.unlink()
            print("Invalid PID file. Removed the stale PID file.")
            return 1

    if args.force:
        pid = find_server_process()
        if pid:
            stop_process(pid)
            print(f"Server stopped (PID: {pid})")
            return 0
        print("No running server process found.")
        return 1

    print("PID file not found. Use --force to search for a running server process.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
