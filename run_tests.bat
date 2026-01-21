@echo off
REM run_tests.bat - Simple batch script to build and run tests

echo === Acoustic Engine Test Runner ===
echo.

cd /d "%~dp0"

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake (try to find Visual Studio)
echo [1/3] Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo Trying Visual Studio 2019...
    cmake .. -G "Visual Studio 16 2019" -A x64
)
if errorlevel 1 (
    echo CMake configuration failed!
    cd ..
    exit /b 1
)

REM Build
echo [2/3] Building...
cmake --build . --config Release
if errorlevel 1 (
    echo Build failed!
    cd ..
    exit /b 1
)

REM Run tests
echo [3/3] Running tests...
echo.

set TOTAL_PASS=0
set TOTAL_FAIL=0

if exist Release\test_math.exe (
    echo --- Running test_math ---
    Release\test_math.exe
    if errorlevel 1 (set /a TOTAL_FAIL+=1) else (set /a TOTAL_PASS+=1)
    echo.
)

if exist Release\test_dsp.exe (
    echo --- Running test_dsp ---
    Release\test_dsp.exe
    if errorlevel 1 (set /a TOTAL_FAIL+=1) else (set /a TOTAL_PASS+=1)
    echo.
)

if exist Release\test_analysis.exe (
    echo --- Running test_analysis ---
    Release\test_analysis.exe
    if errorlevel 1 (set /a TOTAL_FAIL+=1) else (set /a TOTAL_PASS+=1)
    echo.
)

cd ..

echo === Summary ===
echo Passed: %TOTAL_PASS%
echo Failed: %TOTAL_FAIL%

if %TOTAL_FAIL% GTR 0 (
    echo Some tests failed!
    exit /b 1
) else (
    echo All tests passed!
    exit /b 0
)
