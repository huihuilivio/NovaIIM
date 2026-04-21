# Scripts Directory

This directory contains Python scripts for managing the NovaIIM project cross-platform.

## Subproject Targets

| Target | Type | Description |
|--------|------|-------------|
| `server` | C++ (CMake) | IM 服务端 (`im_server`) |
| `desktop` | C++ (CMake) | 桌面客户端 (`nova_desktop`) |
| `sdk` | C++ (CMake) | 客户端 SDK (`nova_sdk`) |
| `admin-web` | Vue3 (npm) | 管理后台前端 (`server/web`) |
| `desktop-web` | Vue3 (npm) | 桌面客户端前端 (`client/desktop/web`) |

## Python Scripts

### 环境与配置

- **`setup_env.py`** — 检查开发环境，报告缺失工具。
  ```bash
  python scripts/setup_env.py
  ```

- **`configure.py`** — 配置 CMake 构建。
  ```bash
  python scripts/configure.py                            # 默认 Release
  python scripts/configure.py --build-type Debug --test  # Debug + 测试
  python scripts/configure.py --no-client                # 仅构建 server
  python scripts/configure.py --mysql                    # 启用 MySQL
  ```

### 构建

- **`build.py`** — 构建子项目（支持按目标选择）。
  ```bash
  python scripts/build.py                    # 构建全部
  python scripts/build.py server             # 仅构建服务端
  python scripts/build.py desktop admin-web  # 桌面客户端 + Admin 前端
  python scripts/build.py --list             # 列出所有目标
  python scripts/build.py --config Debug     # Debug 配置
  ```

- **`production.py`** — 一键生产构建（configure + build）。
  ```bash
  python scripts/production.py                    # 全部
  python scripts/production.py server admin-web   # 服务端 + Admin 前端
  ```

### 启动 / 停止

- **`start.py`** — 启动子项目。
  ```bash
  python scripts/start.py                     # 启动服务端（默认）
  python scripts/start.py server              # 启动服务端
  python scripts/start.py admin-web           # 启动 Admin Web 开发服务器
  python scripts/start.py desktop-web         # 启动桌面前端开发服务器
  python scripts/start.py server admin-web    # 同时启动多个
  ```

- **`stop.py`** — 停止子项目。
  ```bash
  python scripts/stop.py                # 停止服务端（默认）
  python scripts/stop.py desktop        # 停止桌面客户端
  python scripts/stop.py --all          # 停止所有可执行进程
  python scripts/stop.py --force        # 强制搜索并停止
  ```

### 测试 / 覆盖率

- **`test.py`** — 运行 CTest 测试。
  ```bash
  python scripts/test.py
  ```

- **`coverage.py`** — 生成代码覆盖率报告。
  ```bash
  python scripts/coverage.py
  ```

### 部署

- **`deploy.py`** — 部署构建产物到目标目录。
  ```bash
  python scripts/deploy.py                       # 部署全部
  python scripts/deploy.py server                # 仅部署服务端
  python scripts/deploy.py server admin-web      # 服务端 + Admin 前端
  python scripts/deploy.py --clean               # 清空后部署
  ```

### Admin Web 专用

- **`admin_web.py`** — Admin Web 前端开发专用脚本。
  ```bash
  python scripts/admin_web.py install          # 安装 npm 依赖
  python scripts/admin_web.py dev              # 启动开发服务器
  python scripts/admin_web.py build            # 类型检查 + 生产构建
  python scripts/admin_web.py test             # 运行 Vitest 测试
  python scripts/admin_web.py test --watch     # 监听模式
  python scripts/admin_web.py test --coverage  # 带覆盖率
  python scripts/admin_web.py typecheck        # 仅类型检查
  python scripts/admin_web.py preview          # 预览生产构建
  ```

## Prerequisites

- Python 3.8+
- CMake ≥ 3.28
- CTest
- MSVC 2022 or compatible C++ toolchain
- Node.js ≥ 18, npm ≥ 9（前端子项目）
- For coverage: OpenCppCoverage (Windows) or lcov/genhtml (Linux)

Run all scripts from the project root: `python scripts/<script>.py`
