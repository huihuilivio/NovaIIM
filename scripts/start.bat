@echo off
REM Start service script for NovaIIM server (Windows Batch)
REM This script starts the NovaIIM server

set CONFIG_FILE=%1
if "%CONFIG_FILE%"=="" set CONFIG_FILE=.\configs\server.yaml

set LOG_FILE=%2
if "%LOG_FILE%"=="" set LOG_FILE=.\logs\server.log

echo Starting NovaIIM server...

REM Check if server is already running
tasklist /FI "IMAGENAME eq server.exe" 2>NUL | find /I /N "server.exe">NUL
if %errorlevel% equ 0 (
    echo Server is already running.
    exit /b 0
)

REM Create logs directory if it doesn't exist
if not exist ".\logs" (
    mkdir ".\logs"
)

REM Start the server
set SERVER_PATH=.\output\bin\server.exe
if not exist "%SERVER_PATH%" (
    echo Server executable not found at %SERVER_PATH%
    echo Please build the project first.
    exit /b 1
)

start /B "%SERVER_PATH%" "%SERVER_PATH%" --config "%CONFIG_FILE%" > "%LOG_FILE%" 2>&1

REM Save PID for stop script (approximate, since start /B doesn't return PID easily)
for /f "tokens=2" %%i in ('tasklist /FI "IMAGENAME eq server.exe" ^| find "server.exe"') do (
    echo %%i > server.pid
)

echo Server started successfully.
echo Logs: %LOG_FILE%
echo PID saved to server.pid