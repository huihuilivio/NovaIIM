# Configure script for NovaIIM project
# This script configures the CMake build system

param(
    [string]$BuildType = "Release",
    [switch]$EnableCoverage
)

Write-Host "Configuring NovaIIM project..."

$cmakeArgs = @(
    "-S", ".",
    "-B", "build",
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

if ($EnableCoverage) {
    $cmakeArgs += "-DENABLE_COVERAGE=ON"
}

# Run cmake configure
& cmake -S .. -B ../build $cmakeArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Configuration completed successfully."
} else {
    Write-Host "Configuration failed."
}