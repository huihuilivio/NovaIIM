#!/usr/bin/env python3
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
    try:
        if os.name == "nt":
            subprocess.run(["taskkill", "/PID", str(pid), "/F"], check=True)
        else:
            os.kill(pid, signal.SIGTERM)
    except Exception:
        pass


def main() -> int:
    parser = argparse.ArgumentParser(description="Start the NovaIIM server.")
    parser.add_argument("--config-file", default="configs/server.yaml", help="Path to server configuration file.")
    parser.add_argument("--log-file", default="logs/server.log", help="Path to server log file.")
    args = parser.parse_args()

    root = project_root()
    pid_path = root / "server.pid"

    if pid_path.exists():
        try:
            pid = int(pid_path.read_text().strip())
            if is_running(pid):
                print(f"Server is already running (PID: {pid})")
                return 0
            pid_path.unlink()
        except ValueError:
            pid_path.unlink()

    log_path = root / args.log_file
    log_path.parent.mkdir(parents=True, exist_ok=True)

    server_exe = root / "build" / "output" / "bin" / "im_server.exe"
    if not server_exe.exists():
        print(f"Server executable not found at {server_exe}")
        return 1

    config_file = root / args.config_file
    if not config_file.exists():
        print(f"Config file not found: {config_file}")
        return 1

    with open(log_path, "ab") as log_file:
        process = subprocess.Popen(
            [str(server_exe), "--config", str(config_file)],
            stdout=log_file,
            stderr=subprocess.STDOUT,
            cwd=str(root),
            start_new_session=True,
        )

    pid_path.write_text(str(process.pid), encoding="utf-8")
    print(f"Server started successfully (PID: {process.pid})")
    print(f"Logs: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
