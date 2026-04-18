@echo off
REM Environment setup script for NovaIIM project (Windows Batch)
REM This script sets up the development environment

echo Setting up development environment for NovaIIM...

REM Check for CMake
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo CMake not found. Please install CMake from https://cmake.org/
    exit /b 1
)

REM Check for compiler
set FOUND_COMPILER=0
where cl >nul 2>nul && set FOUND_COMPILER=1
if %FOUND_COMPILER% equ 0 (
    where gcc >nul 2>nul && set FOUND_COMPILER=1
)
if %FOUND_COMPILER% equ 0 (
    where clang >nul 2>nul && set FOUND_COMPILER=1
)

if %FOUND_COMPILER% equ 0 (
    echo No C++ compiler found. Please install Visual Studio Build Tools, MinGW, or Clang.
    exit /b 1
) else (
    echo Found C++ compiler.
)

REM Check for Ninja (optional)
where ninja >nul 2>nul
if %errorlevel% equ 0 (
    echo Ninja build system found.
) else (
    echo Ninja not found. CMake will use default generator.
)

REM Check for Git
where git >nul 2>nul
if %errorlevel% neq 0 (
    echo Git not found. Please install Git.
    exit /b 1
)

echo Environment setup completed successfully.