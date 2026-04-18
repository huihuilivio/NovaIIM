#!/bin/bash

# Configure script for NovaIIM project (Linux)
# This script configures the CMake build system

BUILD_TYPE=${1:-Release}
ENABLE_COVERAGE=${2:-OFF}

echo "Configuring NovaIIM project..."

cmake -S .. -B ../build \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DENABLE_COVERAGE=$ENABLE_COVERAGE

if [ $? -eq 0 ]; then
    echo "Configuration completed successfully."
else
    echo "Configuration failed."
    exit 1
fi