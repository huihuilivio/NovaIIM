# Scripts Directory

This directory contains Python scripts for managing the NovaIIM project cross-platform.

## Python Scripts

- `setup_env.py`: Checks the development environment and reports missing tools.
  - Usage: `python scripts/setup_env.py`

- `configure.py`: Configures the CMake build system.
  - Usage: `python scripts/configure.py --build-type Debug`
  - To enable coverage: `python scripts/configure.py --enable-coverage`

- `build.py`: Compiles the project.
  - Usage: `python scripts/build.py --config Release`

- `test.py`: Runs the test suite.
  - Usage: `python scripts/test.py`

- `coverage.py`: Generates a code coverage report.
  - Usage: `python scripts/coverage.py`
  - Requires OpenCppCoverage or lcov/genhtml.

- `production.py`: Builds the project for production.
  - Usage: `python scripts/production.py`

- `deploy.py`: Deploys build artifacts to a target folder.
  - Usage: `python scripts/deploy.py --deploy-path deploy --config Release`

- `start.py`: Starts the NovaIIM server.
  - Usage: `python scripts/start.py --config-file configs/server.yaml --log-file logs/server.log`

- `stop.py`: Stops the NovaIIM server.
  - Usage: `python scripts/stop.py`

- `admin_web.py`: Admin Web 前端开发脚本（install / dev / build / test / typecheck / preview）。
  - Usage:
    ```bash
    python scripts/admin_web.py install     # 安装 npm 依赖
    python scripts/admin_web.py dev         # 启动开发服务器 (port 3000)
    python scripts/admin_web.py build       # 类型检查 + 生产构建
    python scripts/admin_web.py test        # 运行 Vitest 单元测试
    python scripts/admin_web.py test --watch      # 监听模式
    python scripts/admin_web.py test --coverage   # 带覆盖率
    python scripts/admin_web.py typecheck   # 仅 vue-tsc 类型检查
    python scripts/admin_web.py preview     # 预览生产构建产物
    ```
  - 需要 Node.js ≥ 18 和 npm ≥ 9。

## Prerequisites

- Python 3.8+
- CMake
- CTest
- A supported CMake build toolchain (Ninja or Visual Studio Build Tools)
- For coverage: OpenCppCoverage, or lcov/genhtml on supported platforms.

Run scripts from the project root directory with `python scripts/<script>.py`.
