# Scripts Directory

This directory contains scripts for managing the NovaIIM project on Windows (PowerShell and Batch) and Linux (Bash).

## Windows Scripts (.ps1 and .bat)

### PowerShell (.ps1)
- `setup_env.ps1`: Sets up the development environment.
  - Usage: `.\setup_env.ps1`

- `configure.ps1`: Configures the CMake build system.
  - Usage: `.\configure.ps1 -BuildType Debug` or `.\configure.ps1 -EnableCoverage`

- `build.ps1`: Compiles the project.
  - Usage: `.\build.ps1 -Config Release`

- `test.ps1`: Runs the test suite.
  - Usage: `.\test.ps1`

- `coverage.ps1`: Generates code coverage report (requires lcov and gcov).
  - Usage: `.\coverage.ps1`

- `production.ps1`: Builds the project for production.
  - Usage: `.\production.ps1`

- `deploy.ps1`: Deploys the built application.
  - Usage: `.\deploy.ps1 -DeployPath .\deploy -Config Release`

### Batch (.bat)
- `setup_env.bat`: Sets up the development environment.
  - Usage: `setup_env.bat`

- `configure.bat`: Configures the CMake build system.
  - Usage: `configure.bat [Release|Debug] [ON|OFF]`

- `build.bat`: Compiles the project.
  - Usage: `build.bat [Release|Debug]`

- `test.bat`: Runs the test suite.
  - Usage: `test.bat`

- `coverage.bat`: Generates code coverage report (requires lcov and gcov).
  - Usage: `coverage.bat`

- `production.bat`: Builds the project for production.
  - Usage: `production.bat`

- `deploy.bat`: Deploys the built application.
  - Usage: `deploy.bat [deploy_path] [Release|Debug]`

## Linux Scripts (.sh)

- `setup_env.sh`: Sets up the development environment.
  - Usage: `./setup_env.sh`

- `configure.sh`: Configures the CMake build system.
  - Usage: `./configure.sh [Release|Debug] [ON|OFF for coverage]`

- `build.sh`: Compiles the project.
  - Usage: `./build.sh [Release|Debug]`

- `test.sh`: Runs the test suite.
  - Usage: `./test.sh`

- `coverage.sh`: Generates code coverage report (requires lcov and gcov).
  - Usage: `./coverage.sh`

- `production.sh`: Builds the project for production.
  - Usage: `./production.sh`

- `deploy.sh`: Deploys the built application.
  - Usage: `./deploy.sh [deploy_path] [Release|Debug]`

## Prerequisites

### Windows
- PowerShell or Command Prompt
- CMake
- Ninja or Visual Studio Build Tools
- For coverage: lcov, gcov (from GCC)

### Linux
- Bash
- CMake
- Make or Ninja
- For coverage: lcov, gcov (from GCC)

Run scripts from the project root directory. Make sure .sh scripts are executable: `chmod +x scripts/*.sh`