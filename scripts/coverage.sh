#!/bin/bash

# Code coverage script for NovaIIM project (Linux)
# This script generates code coverage report using gcov and lcov

echo "Generating code coverage report for NovaIIM project..."

# Check if lcov is available
if ! command -v lcov &> /dev/null; then
    echo "lcov not found. Please install lcov."
    exit 1
fi

# Navigate to build directory
if [ ! -d "build" ]; then
    echo "Build directory not found. Please build with coverage enabled first."
    exit 1
fi

cd build

# Capture initial coverage
lcov --capture --initial --directory . --output-file coverage_base.info

# Run tests
ctest --output-on-failure

if [ $? -ne 0 ]; then
    echo "Tests failed. Coverage report may be incomplete."
fi

# Capture coverage after tests
lcov --capture --directory . --output-file coverage_test.info

# Combine coverage data
lcov --add-tracefile coverage_base.info --add-tracefile coverage_test.info --output-file coverage_total.info

# Remove external libraries from coverage
lcov --remove coverage_total.info '*/thirdparty/*' '*/_deps/*' '*/build/*' --output-file coverage_filtered.info

# Generate HTML report
genhtml coverage_filtered.info --output-directory coverage_report

echo "Coverage report generated in build/coverage_report/"