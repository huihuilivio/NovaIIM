@echo off
REM Stop service script for NovaIIM server (Windows Batch)
REM This script stops the NovaIIM server

echo Stopping NovaIIM server...

set PID_FILE=server.pid
if not exist "%PID_FILE%" (
    echo PID file not found. Server may not be running.
    REM Try to kill any running server
    taskkill /F /IM server.exe 2>NUL
    if %errorlevel% equ 0 (
        echo Server stopped.
    ) else (
        echo No running server found.
    )
    exit /b 0
)

set /p PID=<"%PID_FILE%"
if "%PID%"=="" (
    echo Invalid PID in file.
    del "%PID_FILE%"
    exit /b 1
)

taskkill /F /PID %PID% 2>NUL
if %errorlevel% equ 0 (
    echo Server stopped (PID: %PID%)
) else (
    echo Failed to stop server with PID %PID%
)

REM Clean up PID file
del "%PID_FILE%"