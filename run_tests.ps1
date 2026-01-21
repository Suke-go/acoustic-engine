#!/usr/bin/env pwsh
# run_tests.ps1 - Script to build and run all tests

$ErrorActionPreference = "Stop"

Write-Host "=== Acoustic Engine Test Runner ===" -ForegroundColor Cyan
Write-Host ""

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# Create build directory
$BuildDir = "build"
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Configure with CMake
Write-Host "[1/3] Configuring with CMake..." -ForegroundColor Yellow
Push-Location $BuildDir
cmake .. -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    Pop-Location
    exit 1
}

# Build
Write-Host "[2/3] Building..." -ForegroundColor Yellow
cmake --build . --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Pop-Location
    exit 1
}

# Run tests
Write-Host "[3/3] Running tests..." -ForegroundColor Yellow
Write-Host ""

$TestResults = @()
$TestExes = @(
    "Release\test_math.exe",
    "Release\test_dsp.exe",
    "Release\test_analysis.exe",
    "Release\test_auditory.exe"
)

foreach ($TestExe in $TestExes) {
    if (Test-Path $TestExe) {
        Write-Host "--- Running $TestExe ---" -ForegroundColor Cyan
        & ".\$TestExe"
        $TestResults += @{Name = $TestExe; ExitCode = $LASTEXITCODE}
        Write-Host ""
    } else {
        Write-Host "Test not found: $TestExe" -ForegroundColor Red
        $TestResults += @{Name = $TestExe; ExitCode = -1}
    }
}

Pop-Location

# Summary
Write-Host "=== Test Summary ===" -ForegroundColor Cyan
$TotalPassed = 0
$TotalFailed = 0

foreach ($Result in $TestResults) {
    $Status = if ($Result.ExitCode -eq 0) { "PASS" } else { "FAIL" }
    $Color = if ($Result.ExitCode -eq 0) { "Green" } else { "Red" }
    Write-Host "  $($Result.Name): $Status" -ForegroundColor $Color
    if ($Result.ExitCode -eq 0) { $TotalPassed++ } else { $TotalFailed++ }
}

Write-Host ""
if ($TotalFailed -eq 0) {
    Write-Host "All tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "$TotalFailed test(s) failed!" -ForegroundColor Red
    exit 1
}
