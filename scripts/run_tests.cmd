@echo off
setlocal
cd /d "%~dp0\.."

if not exist "build\Release\test_text_metrics.exe" (
    echo [WARN] Binaries not found. Building project first...
    call "%~dp0build.cmd"
    if errorlevel 1 exit /b 1
)

echo ========================================================
echo  Running Stage 2 and 3 Tests: Dedup, Text Metrics, Visual Diff
echo ========================================================

echo.
echo [1/3] Running frame deduplication validation...
"build\Release\test_frame_dedup.exe"
if errorlevel 1 (
    echo [ERROR] Frame deduplication test failed!
    pause
    exit /b 1
)

echo.
echo [2/3] Running text metrics validation...
"build\Release\test_text_metrics.exe"
if errorlevel 1 (
    echo [ERROR] Text metrics test failed!
    pause
    exit /b 1
)

echo.
echo [3/4] Running pixel-by-pixel visual diff tool...
"build\Release\visual_diff.exe"
if errorlevel 1 (
    echo [ERROR] Visual diff test failed!
    pause
    exit /b 1
)

echo.
echo [4/5] Running LTTB downsampling validation...
"build\Release\test_lttb.exe"
if errorlevel 1 (
    echo [ERROR] LTTB test failed!
    pause
    exit /b 1
)

echo.
echo [5/5] Running modal dimming order validation...
"build\Release\test_modal.exe"
if errorlevel 1 (
    echo [ERROR] Modal dimming test failed!
    pause
    exit /b 1
)

echo.
echo ========================================================
echo  All tests passed successfully!
echo ========================================================
endlocal
