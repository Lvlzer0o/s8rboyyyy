param(
  [string]$BuildDir = "build",
  [int]$Jobs = 4,
  [switch]$CleanFirst
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-Command {
  param([string]$Name)

  return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Show-InstallInstructions {
  Write-Host ""
  Write-Host "Build tooling is missing. Install required dependencies first." -ForegroundColor Yellow
  Write-Host ""
  Write-Host "Windows:"
  Write-Host "  winget install --id Kitware.CMake -e"
  Write-Host "  winget install --id LLVM.LLVM -e            # provides clang++"
  Write-Host "  winget install --id Microsoft.VisualStudio.2022.BuildTools -e"
  Write-Host "  # or"
  Write-Host "  choco install cmake mingw -y"
  Write-Host ""
  Write-Host "Linux/macOS:"
  Write-Host "  # Ubuntu/Debian"
  Write-Host "  sudo apt install cmake g++ libsdl2-dev libsdl2-ttf-dev libgl1-mesa-dev libglu1-mesa-dev"
  Write-Host ""
}

$missing = @()
if (-not (Test-Command "cmake")) {
  $missing += "cmake"
}

$hasCompiler = (Test-Command "g++") -or (Test-Command "clang++") -or (Test-Command "cl")
if (-not $hasCompiler) {
  $missing += "C++ compiler (g++, clang++, or cl.exe)"
}

if ($missing.Count -gt 0) {
  Write-Host "Missing required tools:" -ForegroundColor Red
  foreach ($tool in $missing) {
    Write-Host "  - $tool" -ForegroundColor Red
  }
  Show-InstallInstructions
  exit 1
}

if (Test-Path $BuildDir) {
  if ($CleanFirst) {
    Remove-Item -Recurse -Force $BuildDir
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
  }
}
else {
  New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Write-Host "Configuring with CMake..."
cmake -S . -B "$BuildDir"

Write-Host "Building..."
if ($CleanFirst) {
  cmake --build "$BuildDir" --clean-first -j $Jobs
} else {
  cmake --build "$BuildDir" -j $Jobs
}
