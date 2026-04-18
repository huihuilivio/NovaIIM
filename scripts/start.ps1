# Start service script for NovaIIM server (PowerShell)
# This script starts the NovaIIM server

param(
    [string]$ConfigFile = ".\configs\server.yaml",
    [string]$LogFile = ".\logs\server.log"
)

Write-Host "Starting NovaIIM server..."

# Check if server is already running
$existingProcess = Get-Process -Name "server" -ErrorAction SilentlyContinue
if ($existingProcess) {
    Write-Host "Server is already running (PID: $($existingProcess.Id))"
    exit 0
}

# Create logs directory if it doesn't exist
$logDir = Split-Path $LogFile -Parent
if (!(Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir | Out-Null
}

# Start the server
$serverPath = ".\output\bin\server.exe"
if (!(Test-Path $serverPath)) {
    Write-Host "Server executable not found at $serverPath"
    Write-Host "Please build the project first."
    exit 1
}

try {
    $process = Start-Process -FilePath $serverPath `
        -ArgumentList "--config", $ConfigFile `
        -RedirectStandardOutput $LogFile `
        -RedirectStandardError $LogFile `
        -NoNewWindow `
        -PassThru

    # Save PID for stop script
    $process.Id | Out-File -FilePath ".\server.pid" -Encoding UTF8

    Write-Host "Server started successfully (PID: $($process.Id))"
    Write-Host "Logs: $LogFile"
} catch {
    Write-Host "Failed to start server: $_"
    exit 1
}