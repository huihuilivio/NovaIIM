#!/bin/bash

# Build script for NovaIIM project (Linux)
# This script compiles the project using CMake

CONFIG=${1:-Release}

echo "Building NovaIIM project in $CONFIG configuration..."

# Navigate to build directory
if [ ! -d "../build" ]; then
    echo "Build directory not found. Please run configure.sh first."
    exit 1
fi

cd ../build

# Build using cmake
cmake --build . --config $CONFIG

if [ $? -eq 0 ]; then
    echo "Build completed successfully."
else
    echo "Build failed."
    exit 1
fi