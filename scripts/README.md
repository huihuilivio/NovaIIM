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

## Prerequisites

- Python 3.8+
- CMake
- CTest
- A supported CMake build toolchain (Ninja or Visual Studio Build Tools)
- For coverage: OpenCppCoverage, or lcov/genhtml on supported platforms.

Run scripts from the project root directory with `python scripts/<script>.py`.
