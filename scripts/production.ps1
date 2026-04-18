# Production build script for NovaIIM project
# This script builds the project in Release configuration for production

Write-Host "Building NovaIIM project for production (Release configuration)..."

# Configure if needed
if (!(Test-Path "../build")) {
    Write-Host "Configuring CMake..."
    & cmake -S .. -B ../build -DCMAKE_BUILD_TYPE=Release
}

# Build
Push-Location "../build"
cmake --build . --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host "Production build completed successfully."
    # Optionally install
    cmake --install . --prefix ../output
} else {
    Write-Host "Production build failed."
}

Pop-Location