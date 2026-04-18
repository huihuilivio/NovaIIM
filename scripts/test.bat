@echo off
REM Test script for NovaIIM project (Windows Batch)
REM This script runs the tests using CTest

echo Running tests for NovaIIM project...

REM Navigate to build directory
if not exist "build" (
    echo Build directory not found. Please build first.
    exit /b 1
)

cd build

REM Run tests using ctest
ctest --output-on-failure

if %errorlevel% equ 0 (
    echo All tests passed.
) else (
    echo Some tests failed.
    exit /b 1
)