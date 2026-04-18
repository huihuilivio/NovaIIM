# Build script for NovaIIM project
# This script compiles the project using CMake

param(
    [string]$Config = "Release"
)

Write-Host "Building NovaIIM project in $Config configuration..."

# Navigate to build directory
if (!(Test-Path "build")) {
    Write-Host "Build directory not found. Please run configure first."
    exit 1
}

Push-Location build

# Build using cmake
cmake --build . --config $Config

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully."
} else {
    Write-Host "Build failed."
}

Pop-Location