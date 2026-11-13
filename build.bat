@echo off
setlocal enabledelayedexpansion

echo ===================================================================
echo   UNIFIED ENGINEERING WORKBENCH - AUTOMATED BUILD SYSTEM
echo   PASS SCIENCE WORKS R^&D Internship 2026-2027
echo ===================================================================

:: Check for CMake
set CMAKE_BIN=cmake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_BIN=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    ) else (
        echo [ERROR] CMake not found. Please install CMake or run from VS Developer Command Prompt.
        exit /b 1
    )
)

:: Check for MSVC environment (cl.exe)
where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [INFO] Initializing Visual Studio x64 Build Environment...
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else (
        echo [WARN] vcvars64.bat not found at default location. Attempting default CMake generator...
    )
)

echo [INFO] Configuring CMake Project (Release)...
"%CMAKE_BIN%" -B build -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo [INFO] Compiling C++ Executables...
"%CMAKE_BIN%" --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    exit /b %ERRORLEVEL%
)

echo ===================================================================
echo [SUCCESS] Build completed successfully!
echo   Binaries generated:
echo     - Launch.exe (Root launcher)
echo     - workbench_server.exe (Backend server)
echo     - build\bin\ (Distribution output)
echo ===================================================================
exit /b 0
