# Test script for NovaIIM project
# This script runs the tests using CTest

Write-Host "Running tests for NovaIIM project..."

# Navigate to build directory
if (!(Test-Path "../build")) {
    Write-Host "Build directory not found. Please build first."
    exit 1
}

Push-Location "../build"

# Run tests using ctest
ctest --output-on-failure

if ($LASTEXITCODE -eq 0) {
    Write-Host "All tests passed."
} else {
    Write-Host "Some tests failed."
}

Pop-Location

Pop-Location