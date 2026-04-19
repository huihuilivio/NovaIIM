#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, which


def main() -> int:
    parser = argparse.ArgumentParser(description="Set up the NovaIIM development environment.")
    parser.add_argument("--check", action="store_true", help="Check required tools without making changes.")
    args = parser.parse_args()

    print("Checking NovaIIM development environment...")
    required = ["cmake", "ctest"]
    missing = []

    for tool in required:
        if not which(tool):
            missing.append(tool)

    if not which("python") and not which("python3"):
        missing.append("python")

    if not missing:
        print("All required tools are available.")
        print("Recommended command: python scripts/configure.py --build-type Release")
        return 0

    print("Missing tools:")
    for tool in missing:
        print(f"  - {tool}")

    print("\nPlease install the missing tools and rerun this script.")
    print("On Windows, install CMake and Visual Studio Build Tools or Ninja.")
    print("On Linux, install CMake, build-essential, and lcov if you need coverage.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
