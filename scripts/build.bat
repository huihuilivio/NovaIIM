@echo off
REM Build script for NovaIIM project (Windows Batch)
REM This script compiles the project using CMake

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

echo Building NovaIIM project in %CONFIG% configuration...

REM Navigate to build directory
if not exist "build" (
    echo Build directory not found. Please run configure.bat first.
    exit /b 1
)

cd build

REM Build using cmake
cmake --build . --config %CONFIG%

if %errorlevel% equ 0 (
    echo Build completed successfully.
) else (
    echo Build failed.
    exit /b 1
)