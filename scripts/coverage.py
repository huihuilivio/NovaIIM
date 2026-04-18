#!/usr/bin/env python3
import argparse
import shutil
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from common import project_root, run, which


def has_tool(name: str) -> bool:
    return which(name) is not None


def configure_build(build_type: str) -> None:
    root = project_root()
    run(["cmake", "-S", str(root), "-B", str(root / "build"), "-DNOVA_BUILD_TESTS=ON", f"-DCMAKE_BUILD_TYPE={build_type}"], cwd=root)


def build_project(build_type: str) -> None:
    root = project_root()
    run(["cmake", "--build", str(root / "build"), "--config", build_type], cwd=root)


def run_open_cpp_coverage(build_type: str, verbose: bool) -> None:
    root = project_root()
    build_dir = root / "build"
    report_dir = build_dir / "coverage_report"
    data_dir = build_dir / "coverage_data"
    report_dir.mkdir(parents=True, exist_ok=True)
    data_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        "OpenCppCoverage",
        "--sources", str(root / "server"),
        "--sources", str(root / "protocol"),
        "--sources", str(root / "client"),
        "--excluded_sources", str(root / "thirdparty"),
        "--excluded_sources", str(root / "build"),
        "--excluded_sources", str(root / "_deps"),
        "--export_type", f"html:{report_dir}",
        "--export_type", f"cobertura:{data_dir / 'coverage.xml'}",
        "--",
        "ctest",
        "--output-on-failure",
        "--build-config",
        build_type,
    ]
    if verbose:
        cmd.append("--verbose")

    run(cmd, cwd=build_dir)
    print(f"Coverage report generated to: {report_dir}")
    print(f"Coverage XML generated to: {data_dir / 'coverage.xml'}")


def run_lcov(build_type: str) -> None:
    root = project_root()
    build_dir = root / "build"
    if not has_tool("lcov") or not has_tool("genhtml"):
        print("Error: lcov or genhtml is not available.")
        raise SystemExit(1)

    run(["lcov", "--capture", "--initial", "--directory", ".", "--output-file", "coverage_base.info"], cwd=build_dir)
    run(["ctest", "--output-on-failure"], cwd=build_dir)
    run(["lcov", "--capture", "--directory", ".", "--output-file", "coverage_test.info"], cwd=build_dir)
    run(["lcov", "--add-tracefile", "coverage_base.info", "--add-tracefile", "coverage_test.info", "--output-file", "coverage_total.info"], cwd=build_dir)
    run(["lcov", "--remove", "coverage_total.info", "*/thirdparty/*", "*/_deps/*", "*/build/*", "--output-file", "coverage_filtered.info"], cwd=build_dir)
    run(["genhtml", "coverage_filtered.info", "--output-directory", "coverage_report"], cwd=build_dir)
    print(f"Coverage report generated to: {build_dir / 'coverage_report'}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate code coverage for NovaIIM.")
    parser.add_argument("--build-config", default="RelWithDebInfo", help="Build configuration.")
    parser.add_argument("--clean-build", action="store_true", help="Remove the build directory before configuring.")
    parser.add_argument("--verbose", action="store_true", help="Enable verbose coverage output.")
    args = parser.parse_args()

    root = project_root()
    build_dir = root / "build"
    if args.clean_build and build_dir.exists():
        shutil.rmtree(build_dir)

    if not build_dir.exists() or not (build_dir / "CMakeCache.txt").exists():
        configure_build(args.build_config)

    build_project(args.build_config)

    if has_tool("OpenCppCoverage"):
        run_open_cpp_coverage(args.build_config, args.verbose)
        return 0

    if has_tool("lcov") and has_tool("genhtml"):
        run_lcov(args.build_config)
        return 0

    print("Error: No supported coverage tool found. Install OpenCppCoverage or lcov/genhtml.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
