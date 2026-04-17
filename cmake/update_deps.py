#!/usr/bin/env python3
"""
NovaIIM 第三方依赖版本检查与更新工具。

用法:
    python cmake/update_deps.py check              # 检查所有依赖的最新版本
    python cmake/update_deps.py update spdlog       # 更新 spdlog 到最新版本
    python cmake/update_deps.py update l8w8jwt 2.6.0  # 更新 l8w8jwt 到指定版本
    python cmake/update_deps.py update-all          # 更新所有依赖（需确认）

依赖分为两类:
  - vendored: 源码存放在 thirdparty/，更新时重新下载并清理
  - fetchcontent: 仅更新 cmake/dependencies.cmake 中的版本号
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import urllib.error
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

# ============================================================
# 项目路径
# ============================================================
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
DEPS_CMAKE = SCRIPT_DIR / "dependencies.cmake"
THIRDPARTY_DIR = PROJECT_ROOT / "thirdparty"


# ============================================================
# 依赖定义
# ============================================================
@dataclass
class Dep:
    """一个第三方依赖的元信息。"""
    name: str                       # 依赖名称 (cmake 变量后缀)
    cmake_var: str                  # cmake 版本变量名
    github_repo: str                # GitHub owner/repo
    vendored: bool = False          # 是否 vendor 到 thirdparty/
    vendor_dir: str = ""            # thirdparty/ 下的目录名
    has_submodules: bool = False    # clone 时是否需要 --recurse-submodules
    # 清理规则: 保留的顶级条目 (文件/目录)
    keep: list[str] = field(default_factory=list)
    # 额外需要创建的空桩目录 (某些 CMakeLists.txt 无条件引用)
    stubs: list[str] = field(default_factory=list)
    # l8w8jwt 特殊: 子模块内也需要清理
    submodule_cleanup: bool = False


DEPS: dict[str, Dep] = {}


def _reg(d: Dep) -> Dep:
    DEPS[d.name] = d
    return d


_reg(Dep(
    name="spdlog",
    cmake_var="NOVA_SPDLOG_VERSION",
    github_repo="gabime/spdlog",
    vendored=True,
    vendor_dir="spdlog",
    keep=["CMakeLists.txt", "LICENSE", "cmake", "include", "src"],
))

_reg(Dep(
    name="libhv",
    cmake_var="NOVA_LIBHV_VERSION",
    github_repo="ithewei/libhv",
))

_reg(Dep(
    name="yalantinglibs",
    cmake_var="NOVA_YALANTINGLIBS_VERSION",
    github_repo="alibaba/yalantinglibs",
))

_reg(Dep(
    name="gtest",
    cmake_var="NOVA_GTEST_VERSION",
    github_repo="google/googletest",
))

_reg(Dep(
    name="cli11",
    cmake_var="NOVA_CLI11_VERSION",
    github_repo="CLIUtils/CLI11",
    vendored=True,
    vendor_dir="cli11",
    keep=["CMakeLists.txt", "LICENSE", "cmake", "include", "src"],
    stubs=["fuzz", "single-include"],
))

_reg(Dep(
    name="ormpp",
    cmake_var="NOVA_ORMPP_VERSION",
    github_repo="qicosmos/ormpp",
))

_reg(Dep(
    name="l8w8jwt",
    cmake_var="NOVA_L8W8JWT_VERSION",
    github_repo="GlitchedPolygons/l8w8jwt",
    vendored=True,
    vendor_dir="l8w8jwt",
    has_submodules=True,
    submodule_cleanup=True,
    keep=["CMakeLists.txt", "LICENSE", "NOTICE", "include", "src", "lib"],
))

# l8w8jwt 子库的清理规则
L8W8JWT_LIB_RULES: dict[str, list[str]] = {
    "checknum":  ["include"],
    "jsmn":      ["jsmn.h"],
    "chillbuff": ["CMakeLists.txt", "include"],
    "mbedtls":   ["CMakeLists.txt", "LICENSE", "3rdparty", "cmake",
                  "framework", "include", "library", "pkgconfig"],
}
# mbedtls 顶层需要删除的额外目录/文件
MBEDTLS_DELETE = [
    ".git", ".github", ".gitattributes", ".gitignore", ".gitmodules",
    ".globalrc", ".mypy.ini", ".pylintrc", ".readthedocs.yaml",
    ".travis.yml", ".uncrustify.cfg",
    "BRANCHES.md", "BUGS.md", "CONTRIBUTING.md", "ChangeLog",
    "ChangeLog.d", "SECURITY.md", "SUPPORT.md", "DartConfiguration.tcl",
    "Makefile", "README.md", "dco.txt",
    "configs", "docs", "doxygen", "programs", "scripts", "tests", "visualc",
]


# ============================================================
# GitHub API 工具
# ============================================================
def github_api(url: str) -> dict | list:
    """GET GitHub API, 返回 JSON。"""
    req = urllib.request.Request(url)
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        req.add_header("Authorization", f"token {token}")
    req.add_header("Accept", "application/vnd.github.v3+json")
    req.add_header("User-Agent", "NovaIIM-dep-updater")
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        if e.code == 403:
            print("  ⚠ GitHub API rate limit. Set GITHUB_TOKEN env var.", file=sys.stderr)
        raise


def get_latest_release(repo: str) -> Optional[str]:
    """获取 GitHub 仓库最新 release tag。"""
    try:
        data = github_api(f"https://api.github.com/repos/{repo}/releases/latest")
        return data.get("tag_name")
    except urllib.error.HTTPError:
        return None


def get_latest_tag(repo: str) -> Optional[str]:
    """获取 GitHub 仓库最新 tag（按创建时间倒序）。"""
    try:
        data = github_api(f"https://api.github.com/repos/{repo}/tags?per_page=1")
        if data:
            return data[0]["name"]
    except urllib.error.HTTPError:
        pass
    return None


def get_latest_version(dep: Dep) -> Optional[str]:
    """获取依赖的最新版本。优先 release，其次 tag。"""
    ver = get_latest_release(dep.github_repo)
    if not ver:
        ver = get_latest_tag(dep.github_repo)
    return ver


# ============================================================
# CMake 版本读写
# ============================================================
def read_cmake_versions() -> dict[str, str]:
    """从 dependencies.cmake 读取所有 NOVA_*_VERSION 变量。"""
    content = DEPS_CMAKE.read_text(encoding="utf-8")
    versions: dict[str, str] = {}
    for m in re.finditer(r'set\((\w+_VERSION)\s+"([^"]+)"\)', content):
        versions[m.group(1)] = m.group(2)
    return versions


def write_cmake_version(cmake_var: str, new_version: str) -> None:
    """更新 dependencies.cmake 中的一个版本变量。"""
    content = DEPS_CMAKE.read_text(encoding="utf-8")
    pattern = rf'(set\({re.escape(cmake_var)}\s+")[^"]+(")'
    new_content, count = re.subn(pattern, rf"\g<1>{new_version}\2", content)
    if count == 0:
        print(f"  ✗ 未找到 {cmake_var}", file=sys.stderr)
        sys.exit(1)
    DEPS_CMAKE.write_text(new_content, encoding="utf-8")


# ============================================================
# 源码清理
# ============================================================
def clean_directory(directory: Path, keep: list[str]) -> None:
    """删除 directory 中不在 keep 列表里的所有文件和目录。"""
    keep_set = set(keep)
    for item in list(directory.iterdir()):
        if item.name not in keep_set:
            if item.is_dir():
                shutil.rmtree(item, ignore_errors=True)
            else:
                item.unlink(missing_ok=True)


def _on_rm_error(func, path, exc_info):
    """Handle read-only files on Windows (e.g., .git pack files)."""
    os.chmod(path, 0o777)
    func(path)


def remove_git_dirs(directory: Path) -> None:
    """递归删除所有 .git 目录。"""
    for git_dir in directory.rglob(".git"):
        if git_dir.is_dir():
            shutil.rmtree(git_dir, onerror=_on_rm_error)


def clean_l8w8jwt(vendor_path: Path) -> None:
    """l8w8jwt 特殊清理: 清理子库目录。"""
    lib_dir = vendor_path / "lib"
    if not lib_dir.exists():
        return

    # 删除不需要的子模块目录
    needed_libs = set(L8W8JWT_LIB_RULES.keys())
    for item in list(lib_dir.iterdir()):
        if item.is_dir() and item.name not in needed_libs:
            shutil.rmtree(item, ignore_errors=True)

    # 清理每个子库
    for lib_name, keep_list in L8W8JWT_LIB_RULES.items():
        lib_path = lib_dir / lib_name
        if lib_path.exists():
            clean_directory(lib_path, keep_list)

    # mbedtls 特殊处理: framework 只保留空 CMakeLists.txt
    fw_dir = lib_dir / "mbedtls" / "framework"
    if fw_dir.exists():
        for item in list(fw_dir.iterdir()):
            if item.name != "CMakeLists.txt":
                if item.is_dir():
                    shutil.rmtree(item, ignore_errors=True)
                else:
                    item.unlink(missing_ok=True)
        # 确保 CMakeLists.txt 存在 (mbedtls 要求)
        cml = fw_dir / "CMakeLists.txt"
        if not cml.exists():
            cml.write_text("# stub\n", encoding="utf-8")

    # mbedtls library/ 内清理非源码
    mbedtls_lib = lib_dir / "mbedtls" / "library"
    if mbedtls_lib.exists():
        for name in [".gitignore", "Makefile"]:
            (mbedtls_lib / name).unlink(missing_ok=True)

    # 3rdparty 清理
    for sub in ["everest", "p256-m"]:
        sub_dir = lib_dir / "mbedtls" / "3rdparty" / sub
        if sub_dir.exists():
            for name in [".git", ".gitignore", "README.md"]:
                p = sub_dir / name
                if p.is_dir():
                    shutil.rmtree(p, ignore_errors=True)
                else:
                    p.unlink(missing_ok=True)
    p256_readme = lib_dir / "mbedtls" / "3rdparty" / "p256-m" / "p256-m" / "README.md"
    p256_readme.unlink(missing_ok=True)
    (lib_dir / "mbedtls" / "3rdparty" / ".gitignore").unlink(missing_ok=True)


def create_stubs(vendor_path: Path, stubs: list[str]) -> None:
    """创建空桩目录 (含空 CMakeLists.txt)。"""
    for stub in stubs:
        stub_dir = vendor_path / stub
        stub_dir.mkdir(parents=True, exist_ok=True)
        cml = stub_dir / "CMakeLists.txt"
        if not cml.exists():
            cml.write_text("", encoding="utf-8")


# ============================================================
# Vendor 更新
# ============================================================
def update_vendor(dep: Dep, version: str) -> None:
    """克隆指定版本源码到 thirdparty/，并清理。"""
    vendor_path = THIRDPARTY_DIR / dep.vendor_dir
    clone_url = f"https://github.com/{dep.github_repo}.git"

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp) / dep.vendor_dir
        print(f"  ↓ Cloning {dep.github_repo} @ {version} ...")
        cmd = ["git", "clone", "--depth=1", "--branch", version, clone_url, str(tmp_path)]
        if dep.has_submodules:
            cmd.insert(2, "--recurse-submodules")
            cmd.insert(3, "--shallow-submodules")
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"  ✗ Clone failed:\n{result.stderr}", file=sys.stderr)
            sys.exit(1)

        # 删除所有 .git 目录
        print("  ✂ Removing .git directories ...")
        remove_git_dirs(tmp_path)

        # 执行清理
        print("  ✂ Cleaning source tree ...")
        clean_directory(tmp_path, dep.keep)

        # l8w8jwt 特殊清理
        if dep.submodule_cleanup:
            clean_l8w8jwt(tmp_path)

        # 创建桩目录
        if dep.stubs:
            create_stubs(tmp_path, dep.stubs)

        # 替换目标目录
        if vendor_path.exists():
            print(f"  ✗ Removing old {vendor_path.relative_to(PROJECT_ROOT)} ...")
            shutil.rmtree(vendor_path, onerror=_on_rm_error)

        print(f"  → Copying to {vendor_path.relative_to(PROJECT_ROOT)} ...")
        shutil.copytree(tmp_path, vendor_path)

    # 统计
    file_count = sum(1 for _ in vendor_path.rglob("*") if _.is_file())
    total_mb = sum(f.stat().st_size for f in vendor_path.rglob("*") if f.is_file()) / (1024 * 1024)
    print(f"  ✓ {file_count} files, {total_mb:.1f} MB")


# ============================================================
# 命令实现
# ============================================================
def cmd_check(args: argparse.Namespace) -> None:
    """检查所有依赖的当前版本与最新版本。"""
    versions = read_cmake_versions()
    print(f"\n{'Dependency':<16} {'Current':<20} {'Latest':<20} {'Status'}")
    print("─" * 76)

    for dep in DEPS.values():
        current = versions.get(dep.cmake_var, "?")
        latest = get_latest_version(dep)
        if latest is None:
            status = "⚠ API error"
        elif current == latest:
            status = "✓ up to date"
        else:
            status = "↑ update available"
        loc = "vendor" if dep.vendored else "fetch"
        print(f"{dep.name:<16} {current:<20} {(latest or '?'):<20} {status}  ({loc})")

    print()


def cmd_update(args: argparse.Namespace) -> None:
    """更新指定依赖。"""
    name = args.dep.lower()
    if name not in DEPS:
        print(f"Unknown dependency: {name}")
        print(f"Available: {', '.join(DEPS.keys())}")
        sys.exit(1)

    dep = DEPS[name]
    versions = read_cmake_versions()
    current = versions.get(dep.cmake_var, "?")

    if args.version:
        new_version = args.version
    else:
        print(f"Checking latest version for {dep.name} ...")
        new_version = get_latest_version(dep)
        if not new_version:
            print(f"  ✗ Could not determine latest version for {dep.github_repo}")
            sys.exit(1)

    print(f"\n{dep.name}: {current} → {new_version}")

    if current == new_version and not args.force:
        print("  Already at this version. Use --force to re-vendor.")
        return

    if dep.vendored:
        update_vendor(dep, new_version)

    write_cmake_version(dep.cmake_var, new_version)
    print(f"  ✓ Updated {dep.cmake_var} in dependencies.cmake")
    print(f"\n⚠ Run cmake reconfigure + build + test to verify.")


def cmd_update_all(args: argparse.Namespace) -> None:
    """更新所有依赖到最新版本。"""
    versions = read_cmake_versions()
    updates: list[tuple[Dep, str, str]] = []

    print("Checking latest versions ...")
    for dep in DEPS.values():
        current = versions.get(dep.cmake_var, "?")
        latest = get_latest_version(dep)
        if latest and latest != current:
            updates.append((dep, current, latest))
            print(f"  {dep.name}: {current} → {latest}")
        else:
            print(f"  {dep.name}: {current} (up to date)")

    if not updates:
        print("\nAll dependencies are up to date.")
        return

    print(f"\n{len(updates)} update(s) available.")
    answer = input("Proceed? [y/N] ").strip().lower()
    if answer != "y":
        print("Cancelled.")
        return

    for dep, current, latest in updates:
        print(f"\n{'='*60}")
        print(f"Updating {dep.name}: {current} → {latest}")
        print(f"{'='*60}")
        if dep.vendored:
            update_vendor(dep, latest)
        write_cmake_version(dep.cmake_var, latest)
        print(f"  ✓ Updated {dep.cmake_var}")

    print(f"\n✓ All updates applied.")
    print(f"⚠ Run cmake reconfigure + build + test to verify.")


# ============================================================
# CLI
# ============================================================
def main() -> None:
    parser = argparse.ArgumentParser(
        description="NovaIIM 第三方依赖版本管理工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("check", help="检查所有依赖的最新版本")

    p_update = sub.add_parser("update", help="更新指定依赖")
    p_update.add_argument("dep", help="依赖名称 (spdlog/libhv/cli11/...)")
    p_update.add_argument("version", nargs="?", help="目标版本 (默认: 最新)")
    p_update.add_argument("--force", action="store_true", help="即使版本相同也重新 vendor")

    sub.add_parser("update-all", help="更新所有依赖到最新版本")

    args = parser.parse_args()

    if args.command == "check":
        cmd_check(args)
    elif args.command == "update":
        cmd_update(args)
    elif args.command == "update-all":
        cmd_update_all(args)


if __name__ == "__main__":
    # Windows console encoding fix
    if sys.platform == "win32":
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[union-attr]
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[union-attr]
    main()
