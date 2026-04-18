#!/bin/bash

# Test script for NovaIIM project (Linux)
# This script runs the tests using CTest

echo "Running tests for NovaIIM project..."

# Navigate to build directory
if [ ! -d "build" ]; then
    echo "Build directory not found. Please build first."
    exit 1
fi

cd build

# Run tests using ctest
ctest --output-on-failure

if [ $? -eq 0 ]; then
    echo "All tests passed."
else
    echo "Some tests failed."
    exit 1
fi