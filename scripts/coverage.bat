@echo off
REM Code coverage script for NovaIIM project (Windows Batch)
REM This script generates code coverage report using gcov and lcov

echo Generating code coverage report for NovaIIM project...

REM Check if lcov is available
where lcov >nul 2>nul
if %errorlevel% neq 0 (
    echo lcov not found. Please install lcov.
    exit /b 1
)

REM Navigate to build directory
if not exist "build" (
    echo Build directory not found. Please build with coverage enabled first.
    exit /b 1
)

cd build

REM Capture initial coverage
lcov --capture --initial --directory . --output-file coverage_base.info

REM Run tests
ctest --output-on-failure

if %errorlevel% neq 0 (
    echo Tests failed. Coverage report may be incomplete.
)

REM Capture coverage after tests
lcov --capture --directory . --output-file coverage_test.info

REM Combine coverage data
lcov --add-tracefile coverage_base.info --add-tracefile coverage_test.info --output-file coverage_total.info

REM Remove external libraries from coverage
lcov --remove coverage_total.info */thirdparty/* */_deps/* */build/* --output-file coverage_filtered.info

REM Generate HTML report
genhtml coverage_filtered.info --output-directory coverage_report

echo Coverage report generated in build\coverage_report\