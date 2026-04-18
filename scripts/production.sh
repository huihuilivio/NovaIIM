#!/bin/bash

# Production build script for NovaIIM project (Linux)
# This script builds the project in Release configuration for production

echo "Building NovaIIM project for production (Release configuration)..."

# Configure if needed
if [ ! -d "build" ]; then
    echo "Configuring CMake..."
    ./scripts/configure.sh Release OFF
fi

# Build
cd build
cmake --build . --config Release

if [ $? -eq 0 ]; then
    echo "Production build completed successfully."
    # Optionally install
    cmake --install . --prefix ../output
else
    echo "Production build failed."
    exit 1
fi