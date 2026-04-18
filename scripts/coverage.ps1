# Code coverage script for NovaIIM project
# This script generates code coverage report using gcov and lcov

Write-Host "Generating code coverage report for NovaIIM project..."

# Check if lcov is available
if (!(Get-Command lcov -ErrorAction SilentlyContinue)) {
    Write-Host "lcov not found. Please install lcov."
    exit 1
}

# Navigate to build directory
if (!(Test-Path "../build")) {
    Write-Host "Build directory not found. Please build with coverage enabled first."
    exit 1
}

Push-Location "../build"

# Capture initial coverage
lcov --capture --initial --directory . --output-file coverage_base.info

# Run tests
ctest --output-on-failure

if ($LASTEXITCODE -ne 0) {
    Write-Host "Tests failed. Coverage report may be incomplete."
}

# Capture coverage after tests
lcov --capture --directory . --output-file coverage_test.info

# Combine coverage data
lcov --add-tracefile coverage_base.info --add-tracefile coverage_test.info --output-file coverage_total.info

# Remove external libraries from coverage
lcov --remove coverage_total.info '*/thirdparty/*' '*/_deps/*' '*/build/*' --output-file coverage_filtered.info

# Generate HTML report
genhtml coverage_filtered.info --output-directory coverage_report

Write-Host "Coverage report generated in build/coverage_report/"

Pop-Location