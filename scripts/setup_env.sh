#!/bin/bash

# Environment setup script for NovaIIM project (Linux)
# This script sets up the development environment

echo "Setting up development environment for NovaIIM..."

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "CMake not found. Please install CMake."
    echo "On Ubuntu/Debian: sudo apt install cmake"
    echo "On CentOS/RHEL: sudo yum install cmake"
    exit 1
fi

# Check for compiler
FOUND_COMPILER=0
if command -v g++ &> /dev/null; then
    echo "Found compiler: g++"
    FOUND_COMPILER=1
elif command -v clang++ &> /dev/null; then
    echo "Found compiler: clang++"
    FOUND_COMPILER=1
fi

if [ $FOUND_COMPILER -eq 0 ]; then
    echo "No C++ compiler found. Please install GCC or Clang."
    echo "On Ubuntu/Debian: sudo apt install build-essential"
    echo "On CentOS/RHEL: sudo yum groupinstall 'Development Tools'"
    exit 1
fi

# Check for Ninja (optional)
if command -v ninja &> /dev/null; then
    echo "Ninja build system found."
else
    echo "Ninja not found. CMake will use default generator."
fi

# Check for Git
if ! command -v git &> /dev/null; then
    echo "Git not found. Please install Git."
    exit 1
fi

echo "Environment setup completed successfully."