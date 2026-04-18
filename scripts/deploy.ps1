# Deployment script for NovaIIM project (PowerShell)
# This script deploys the built application

param(
    [string]$DeployPath = ".\deploy",
    [string]$Config = "Release"
)

Write-Host "Deploying NovaIIM project to $DeployPath..."

# Check if build exists
if (!(Test-Path "../build")) {
    Write-Host "Build directory not found. Please build first."
    exit 1
}

# Create deploy directory
if (!(Test-Path $DeployPath)) {
    New-Item -ItemType Directory -Path $DeployPath | Out-Null
}

# Copy binaries
$binSource = Join-Path "../build" "output\bin"
if (Test-Path $binSource) {
    Copy-Item "$binSource\*" $DeployPath -Recurse -Force
}

# Copy libraries
$libSource = Join-Path "../build" "output\lib"
if (Test-Path $libSource) {
    Copy-Item "$libSource\*" $DeployPath -Recurse -Force
}

# Copy config files
if (Test-Path "configs") {
    Copy-Item "configs\*" $DeployPath -Recurse -Force
}

# Copy documentation
if (Test-Path "README.md") {
    Copy-Item "README.md" $DeployPath
}

Write-Host "Deployment completed successfully to $DeployPath"

# Optional: Create start script
$startScript = @"
@echo off
REM Start script for NovaIIM
echo Starting NovaIIM server...
REM Add your server startup command here
REM Example: server.exe --config config.yaml
pause
"@

$startScript | Out-File -FilePath (Join-Path $DeployPath "start.bat") -Encoding UTF8

Write-Host "Created start.bat in deploy directory."