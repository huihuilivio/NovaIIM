@echo off
REM Production build script for NovaIIM project (Windows Batch)
REM This script builds the project in Release configuration for production

echo Building NovaIIM project for production (Release configuration)...

REM Configure if needed
if not exist "build" (
    echo Configuring CMake...
    call ..\scripts\configure.bat Release OFF
)

REM Build
cd build
cmake --build . --config Release

if %errorlevel% equ 0 (
    echo Production build completed successfully.
    REM Optionally install
    cmake --install . --prefix ..\output
) else (
    echo Production build failed.
    exit /b 1
)