@echo off
setlocal
cd /d "%~dp0\.."

if not exist "build\Release\test_text_metrics.exe" (
    echo [WARN] Binaries not found. Building project first...
    call "%~dp0build.cmd"
    if errorlevel 1 exit /b 1
)

echo ========================================================
echo  Running Stage 3 Tests: Text Metrics and Visual Diff
echo ========================================================

echo.
echo [1/2] Running text metrics validation...
"build\Release\test_text_metrics.exe"
if errorlevel 1 (
    echo [ERROR] Text metrics test failed!
    pause
    exit /b 1
)

echo.
echo [2/2] Running pixel-by-pixel visual diff tool...
"build\Release\visual_diff.exe"
if errorlevel 1 (
    echo [ERROR] Visual diff test failed!
    pause
    exit /b 1
)

echo.
echo ========================================================
echo  All tests passed successfully!
echo ========================================================
endlocal
