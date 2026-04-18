# Environment setup script for NovaIIM project (PowerShell)
# This script sets up the development environment

Write-Host "Setting up development environment for NovaIIM..."

# Check for CMake
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "CMake not found. Please install CMake from https://cmake.org/"
    exit 1
}

# Check for compiler
$compilers = @("cl", "gcc", "clang")
$foundCompiler = $false
foreach ($compiler in $compilers) {
    if (Get-Command $compiler -ErrorAction SilentlyContinue) {
        Write-Host "Found compiler: $compiler"
        $foundCompiler = $true
        break
    }
}

if (!$foundCompiler) {
    Write-Host "No C++ compiler found. Please install Visual Studio Build Tools, MinGW, or Clang."
    exit 1
}

# Check for Ninja (optional)
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    Write-Host "Ninja build system found."
} else {
    Write-Host "Ninja not found. CMake will use default generator."
}

# Check for Git
if (!(Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "Git not found. Please install Git."
    exit 1
}

Write-Host "Environment setup completed successfully."