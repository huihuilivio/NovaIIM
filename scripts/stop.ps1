# Stop service script for NovaIIM server (PowerShell)
# This script stops the NovaIIM server

Write-Host "Stopping NovaIIM server..."

$pidFile = ".\server.pid"
if (!(Test-Path $pidFile)) {
    Write-Host "PID file not found. Server may not be running."
    # Try to find running server process
    $process = Get-Process -Name "server" -ErrorAction SilentlyContinue
    if ($process) {
        Write-Host "Found running server (PID: $($process.Id)). Stopping..."
        Stop-Process -Id $process.Id -Force
        Write-Host "Server stopped."
    } else {
        Write-Host "No running server found."
    }
    exit 0
}

try {
    $pid = Get-Content $pidFile -Raw
    $pid = $pid.Trim()

    if ($pid -match '^\d+$') {
        $process = Get-Process -Id $pid -ErrorAction SilentlyContinue
        if ($process) {
            Stop-Process -Id $pid -Force
            Write-Host "Server stopped (PID: $pid)"
        } else {
            Write-Host "Process with PID $pid not found."
        }
    } else {
        Write-Host "Invalid PID in file."
    }
} catch {
    Write-Host "Error stopping server: $_"
} finally {
    # Clean up PID file
    if (Test-Path $pidFile) {
        Remove-Item $pidFile
    }
}