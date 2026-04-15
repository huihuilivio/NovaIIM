#!/usr/bin/env python3
"""
fetch_mysql_client.py - 检测 / 下载 MySQL 客户端库

用法:
    python fetch_mysql_client.py <install_dir> [--version X.Y.Z]

流程:
    1. 检查 install_dir 是否已有缓存
    2. 检测系统已安装的 MySQL / MariaDB 客户端
    3. Linux/macOS 尝试包管理器安装
    4. 下载当前平台对应的 MySQL Community Server 预编译包，提取 include/ 和 lib/

输出 (stdout, JSON):
    {"include_dir": "...", "lib_dir": "..."}
"""

import sys
import os
import platform
import subprocess
import json
import urllib.request
import zipfile
import tarfile
import shutil
import argparse
from pathlib import Path

MYSQL_DEFAULT_VERSION = "8.0.41"
MYSQL_DEFAULT_MAJOR = "8.0"


def log(msg):
    print(f"[fetch_mysql] {msg}", file=sys.stderr, flush=True)


# ────────────────────────────────────────────────────────────
# 1. 检测系统已安装的 MySQL
# ────────────────────────────────────────────────────────────

def _find_via_config():
    """通过 mysql_config / mariadb_config 查找。"""
    for cmd in ("mysql_config", "mariadb_config"):
        try:
            inc = subprocess.run(
                [cmd, "--variable=pkgincludedir"],
                capture_output=True, text=True, timeout=5,
            )
            lib = subprocess.run(
                [cmd, "--variable=pkglibdir"],
                capture_output=True, text=True, timeout=5,
            )
            if inc.returncode == 0 and lib.returncode == 0:
                inc_dir = inc.stdout.strip()
                lib_dir = lib.stdout.strip()
                if os.path.isdir(inc_dir) and os.path.isdir(lib_dir):
                    log(f"Found via {cmd}: include={inc_dir}, lib={lib_dir}")
                    return {"include_dir": inc_dir, "lib_dir": lib_dir}
        except (FileNotFoundError, subprocess.TimeoutExpired):
            pass
    return None


def _find_windows():
    search_bases = []
    for env in ("MYSQL_ROOT", "MYSQL_DIR"):
        val = os.environ.get(env)
        if val:
            search_bases.append(val)
    search_bases += [
        r"C:\Program Files\MySQL\MySQL Server 8.4",
        r"C:\Program Files\MySQL\MySQL Server 8.0",
        r"C:\Program Files\MySQL\MySQL Server 5.7",
        r"C:\Program Files (x86)\MySQL\MySQL Server 8.0",
        r"C:\Program Files\MariaDB\MariaDB Connector C",
    ]
    for base in search_bases:
        inc_dir = os.path.join(base, "include")
        lib_dir = os.path.join(base, "lib")
        has_header = os.path.isfile(os.path.join(inc_dir, "mysql.h"))
        has_lib = (
            os.path.isfile(os.path.join(lib_dir, "libmysql.lib"))
            or os.path.isfile(os.path.join(lib_dir, "mysqlclient.lib"))
        )
        if has_header and has_lib:
            log(f"Found MySQL at {base}")
            return {"include_dir": inc_dir, "lib_dir": lib_dir}
    return None


def _find_linux():
    search = [
        ("/usr/include/mysql", ["/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/usr/lib"]),
        ("/usr/include/mariadb", ["/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/usr/lib"]),
        ("/usr/local/include/mysql", ["/usr/local/lib"]),
    ]
    for inc_dir, lib_dirs in search:
        if not os.path.isfile(os.path.join(inc_dir, "mysql.h")):
            continue
        for lib_dir in lib_dirs:
            for name in ("libmysqlclient.so", "libmysqlclient.a", "libmariadb.so"):
                if os.path.isfile(os.path.join(lib_dir, name)):
                    log(f"Found MySQL: include={inc_dir}, lib={lib_dir}")
                    return {"include_dir": inc_dir, "lib_dir": lib_dir}
    return None


def _find_macos():
    prefixes = [
        "/opt/homebrew/opt/mysql-client",
        "/opt/homebrew/opt/mysql",
        "/usr/local/opt/mysql-client",
        "/usr/local/opt/mysql",
    ]
    for prefix in prefixes:
        for sub in ("include/mysql", "include"):
            inc_dir = os.path.join(prefix, sub)
            lib_dir = os.path.join(prefix, "lib")
            if os.path.isfile(os.path.join(inc_dir, "mysql.h")) and os.path.isdir(lib_dir):
                log(f"Found MySQL at {prefix}")
                return {"include_dir": inc_dir, "lib_dir": lib_dir}
    return None


def find_system_mysql():
    """在系统中查找已安装的 MySQL 客户端库。"""
    result = _find_via_config()
    if result:
        return result

    system = platform.system()
    if system == "Windows":
        return _find_windows()
    elif system == "Linux":
        return _find_linux()
    elif system == "Darwin":
        return _find_macos()
    return None


# ────────────────────────────────────────────────────────────
# 2. 包管理器安装 (Linux / macOS)
# ────────────────────────────────────────────────────────────

def try_package_manager():
    """尝试用系统包管理器安装 MySQL 客户端开发库。"""
    system = platform.system()

    if system == "Linux":
        if shutil.which("apt-get"):
            log("Trying: apt-get install libmysqlclient-dev ...")
            r = subprocess.run(
                ["sudo", "apt-get", "install", "-y", "libmysqlclient-dev"],
                capture_output=True, text=True, timeout=120,
            )
            if r.returncode == 0:
                return find_system_mysql()

        for mgr in ("dnf", "yum"):
            if shutil.which(mgr):
                log(f"Trying: {mgr} install mysql-devel ...")
                r = subprocess.run(
                    ["sudo", mgr, "install", "-y", "mysql-devel"],
                    capture_output=True, text=True, timeout=120,
                )
                if r.returncode == 0:
                    return find_system_mysql()

    elif system == "Darwin":
        if shutil.which("brew"):
            log("Trying: brew install mysql-client ...")
            r = subprocess.run(
                ["brew", "install", "mysql-client"],
                capture_output=True, text=True, timeout=300,
            )
            if r.returncode == 0:
                return find_system_mysql()

    return None


# ────────────────────────────────────────────────────────────
# 3. 下载预编译 MySQL 客户端
# ────────────────────────────────────────────────────────────

def _download_urls(version, major):
    """根据平台返回 [(url, archive_type, prefix), ...]，多个候选源按优先级排列。"""
    system = platform.system()
    machine = platform.machine().lower()

    # 多镜像源 (archives CDN 最稳定；Downloads 为当前版本；dev.mysql.com 常 403)
    bases = [
        f"https://cdn.mysql.com/archives/mysql-{major}",
        f"https://cdn.mysql.com/Downloads/MySQL-{major}",
        f"https://dev.mysql.com/get/Downloads/MySQL-{major}",
    ]

    filename = None
    atype = None
    prefix = None

    if system == "Windows" and machine in ("amd64", "x86_64"):
        filename = f"mysql-{version}-winx64.zip"
        atype = "zip"
        prefix = f"mysql-{version}-winx64"
    elif system == "Linux":
        if machine == "x86_64":
            filename = f"mysql-{version}-linux-glibc2.17-x86_64.tar.xz"
            atype = "tar.xz"
            prefix = f"mysql-{version}-linux-glibc2.17-x86_64"
        elif machine == "aarch64":
            filename = f"mysql-{version}-linux-glibc2.17-aarch64.tar.xz"
            atype = "tar.xz"
            prefix = f"mysql-{version}-linux-glibc2.17-aarch64"
    elif system == "Darwin":
        if machine == "arm64":
            filename = f"mysql-{version}-macos14-arm64.tar.gz"
            atype = "tar.gz"
            prefix = f"mysql-{version}-macos14-arm64"
        else:
            filename = f"mysql-{version}-macos14-x86_64.tar.gz"
            atype = "tar.gz"
            prefix = f"mysql-{version}-macos14-x86_64"

    if filename is None:
        return []

    results = []
    for base in bases:
        # archives 镜像直接拼接文件名即可
        results.append((f"{base}/{filename}", atype, prefix))
    return results


def _download_file(url, dest):
    log(f"Downloading {url} ...")
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=600) as resp:
        total = int(resp.headers.get("Content-Length", 0))
        downloaded = 0
        blk = 1024 * 1024
        with open(dest, "wb") as f:
            while True:
                chunk = resp.read(blk)
                if not chunk:
                    break
                f.write(chunk)
                downloaded += len(chunk)
                if total > 0:
                    log(f"  {downloaded // (1024*1024)} / {total // (1024*1024)} MB "
                        f"({downloaded * 100 // total}%)")
    log("Download complete.")


# 需要提取的库文件后缀
_LIB_EXTS = (".lib", ".dll", ".a", ".so", ".dylib")


def _want_file(rel_path):
    """判断归档中的文件是否需要提取 (include/ 全量 + lib/ 的库文件)。"""
    if rel_path.startswith("include/"):
        return True
    if rel_path.startswith("lib/"):
        base = os.path.basename(rel_path)
        if any(base.endswith(e) for e in _LIB_EXTS) or ".so." in base:
            return True
    return False


def _extract_zip(archive, prefix, install_dir):
    with zipfile.ZipFile(archive) as zf:
        for member in zf.namelist():
            if member.endswith("/"):
                continue
            rel = member
            if rel.startswith(prefix + "/"):
                rel = rel[len(prefix) + 1:]
            if not _want_file(rel):
                continue
            target = os.path.join(install_dir, rel)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with zf.open(member) as src, open(target, "wb") as dst:
                shutil.copyfileobj(src, dst)


def _extract_tar(archive, archive_type, prefix, install_dir):
    mode = "r:xz" if archive_type == "tar.xz" else "r:gz"
    with tarfile.open(archive, mode) as tf:
        for member in tf.getmembers():
            if not member.isfile():
                continue
            rel = member.name
            if rel.startswith(prefix + "/"):
                rel = rel[len(prefix) + 1:]
            if not _want_file(rel):
                continue
            source = tf.extractfile(member)
            if not source:
                continue
            target = os.path.join(install_dir, rel)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, "wb") as dst:
                shutil.copyfileobj(source, dst)


def download_mysql(install_dir, version, major):
    candidates = _download_urls(version, major)
    if not candidates:
        log(f"ERROR: No pre-built MySQL available for "
            f"{platform.system()}/{platform.machine()}")
        sys.exit(1)

    # 所有候选共享同样的 archive_type 和 prefix
    _, atype, prefix = candidates[0]
    ext = {"zip": ".zip", "tar.xz": ".tar.xz", "tar.gz": ".tar.gz"}[atype]
    archive = os.path.join(install_dir, f"mysql-{version}{ext}")

    os.makedirs(install_dir, exist_ok=True)

    if not os.path.isfile(archive):
        last_err = None
        for url, _, _ in candidates:
            try:
                _download_file(url, archive)
                last_err = None
                break
            except Exception as e:
                last_err = e
                log(f"Mirror failed ({e}), trying next ...")
                # 删除可能写了一半的文件
                if os.path.isfile(archive):
                    os.remove(archive)
        if last_err is not None:
            log(f"ERROR: All download mirrors failed. Last error: {last_err}")
            sys.exit(1)

    log(f"Extracting client files to {install_dir} ...")
    if atype == "zip":
        _extract_zip(archive, prefix, install_dir)
    else:
        _extract_tar(archive, atype, prefix, install_dir)

    # 删除归档节省磁盘
    try:
        os.remove(archive)
        log("Removed archive.")
    except OSError:
        pass

    inc_dir = os.path.join(install_dir, "include")
    lib_dir = os.path.join(install_dir, "lib")
    if not os.path.isfile(os.path.join(inc_dir, "mysql.h")):
        log("ERROR: mysql.h not found after extraction")
        sys.exit(1)
    return {"include_dir": inc_dir, "lib_dir": lib_dir}


# ────────────────────────────────────────────────────────────
# 主流程
# ────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Detect or download MySQL client library")
    parser.add_argument("install_dir", help="Directory to cache downloaded MySQL files")
    parser.add_argument("--version", default=MYSQL_DEFAULT_VERSION,
                        help=f"MySQL version (default: {MYSQL_DEFAULT_VERSION})")
    parser.add_argument("--major", default=MYSQL_DEFAULT_MAJOR,
                        help=f"MySQL major version (default: {MYSQL_DEFAULT_MAJOR})")
    args = parser.parse_args()

    install_dir = os.path.abspath(args.install_dir)

    # 1) 缓存命中
    cached_header = os.path.join(install_dir, "include", "mysql.h")
    cached_lib = os.path.join(install_dir, "lib")
    if os.path.isfile(cached_header) and os.path.isdir(cached_lib):
        log("Using cached MySQL client library.")
        print(json.dumps({
            "include_dir": os.path.join(install_dir, "include"),
            "lib_dir": cached_lib,
        }))
        return

    # 2) 系统已安装
    log("Checking for existing MySQL client library ...")
    found = find_system_mysql()
    if found:
        print(json.dumps(found))
        return

    # 3) 包管理器 (Linux / macOS)
    if platform.system() in ("Linux", "Darwin"):
        log("Trying package manager ...")
        found = try_package_manager()
        if found:
            print(json.dumps(found))
            return

    # 4) 下载预编译包
    log("Downloading pre-built MySQL client library ...")
    result = download_mysql(install_dir, args.version, args.major)
    print(json.dumps(result))
    log("MySQL client library is ready.")


if __name__ == "__main__":
    main()
