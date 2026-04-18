@echo off
REM Deployment script for NovaIIM project (Windows Batch)
REM This script deploys the built application

set DEPLOY_PATH=%1
if "%DEPLOY_PATH%"=="" set DEPLOY_PATH=.\deploy

set CONFIG=%2
if "%CONFIG%"=="" set CONFIG=Release

echo Deploying NovaIIM project to %DEPLOY_PATH%...

REM Check if build exists
if not exist "build" (
    echo Build directory not found. Please build first.
    exit /b 1
)

REM Create deploy directory
if not exist "%DEPLOY_PATH%" (
    mkdir "%DEPLOY_PATH%"
)

REM Copy binaries
if exist "build\output\bin" (
    xcopy "build\output\bin\*" "%DEPLOY_PATH%\" /E /I /Y
)

REM Copy libraries
if exist "build\output\lib" (
    xcopy "build\output\lib\*" "%DEPLOY_PATH%\" /E /I /Y
)

REM Copy config files
if exist "configs" (
    xcopy "configs\*" "%DEPLOY_PATH%\" /E /I /Y
)

REM Copy documentation
if exist "README.md" (
    copy "README.md" "%DEPLOY_PATH%\"
)

echo Deployment completed successfully to %DEPLOY_PATH%

REM Create start script
echo @echo off > "%DEPLOY_PATH%\start.bat"
echo REM Start script for NovaIIM >> "%DEPLOY_PATH%\start.bat"
echo echo Starting NovaIIM server... >> "%DEPLOY_PATH%\start.bat"
echo REM Add your server startup command here >> "%DEPLOY_PATH%\start.bat"
echo REM Example: server.exe --config config.yaml >> "%DEPLOY_PATH%\start.bat"
echo pause >> "%DEPLOY_PATH%\start.bat"

echo Created start.bat in deploy directory.