@echo off
REM Configure script for NovaIIM project (Windows Batch)
REM This script configures the CMake build system

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

set ENABLE_COVERAGE=%2
if "%ENABLE_COVERAGE%"=="" set ENABLE_COVERAGE=OFF

echo Configuring NovaIIM project...

cmake -S . -B build -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DENABLE_COVERAGE=%ENABLE_COVERAGE%

if %errorlevel% equ 0 (
    echo Configuration completed successfully.
) else (
    echo Configuration failed.
    exit /b 1
)