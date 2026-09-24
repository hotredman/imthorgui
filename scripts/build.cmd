@echo off
setlocal
cd /d "%~dp0\.."

echo ========================================================
echo  Building imthorgui (Release)
echo ========================================================

if not exist build (
    echo [INFO] Generating CMake build configuration...
    cmake -B build -S .
    if errorlevel 1 (
        echo [ERROR] CMake configuration failed!
        pause
        exit /b 1
    )
)

echo [INFO] Compiling targets (Release)...
cmake --build build --config Release
if errorlevel 1 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================================
echo  Build succeeded!
echo  Binaries located in: build\Release\
echo   - demo.exe
echo   - test_text_metrics.exe
echo   - visual_diff.exe
echo ========================================================
endlocal
