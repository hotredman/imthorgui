@echo off
setlocal
cd /d "%~dp0\.."

if not exist "build\Release\demo.exe" (
    echo [WARN] demo.exe not found. Building project first...
    call "%~dp0build.cmd"
    if errorlevel 1 (
        echo [ERROR] Build failed, cannot launch demo.
        pause
        exit /b 1
    )
)

echo ========================================================
echo  Launching ImGui Vector Backend Demo...
echo ========================================================
start "" "build\Release\demo.exe" %*
endlocal
