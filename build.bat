@echo off
setlocal enabledelayedexpansion

echo ===================================================================
echo   UNIFIED ENGINEERING WORKBENCH - AUTOMATED BUILD SYSTEM
echo   PASS SCIENCE WORKS R^&D Internship 2026-2027
echo ===================================================================

:: 1. Ensure MSVC C++ toolchain is available
where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 goto :find_cmake

echo [INFO] Searching for Visual Studio installation via vswhere...
set "VS_DIR="
set "VSWHERE_DIR=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if not exist "%VSWHERE_DIR%\vswhere.exe" goto :check_vcvars

pushd "%VSWHERE_DIR%"
for /f "usebackq tokens=*" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_DIR=%%i"
)
popd

:check_vcvars
if not defined VS_DIR goto :no_compiler

if not exist "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" goto :try_vcvarsall
echo [INFO] Initializing x64 environment via vcvars64.bat...
call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
goto :check_cl

:try_vcvarsall
if not exist "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" goto :no_compiler
echo [INFO] Initializing x64 environment via vcvarsall.bat x64...
call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
goto :check_cl

:check_cl
where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 goto :find_cmake

:no_compiler
echo [ERROR] Microsoft C++ x64 compiler (cl.exe) not found.
echo Please run build.bat from a "x64 Native Tools Command Prompt for VS" or install MSVC C++ build tools.
exit /b 1

:find_cmake
set "CMAKE_BIN=cmake"
where cmake.exe >nul 2>&1
if %ERRORLEVEL% equ 0 goto :configure_cmake

if defined VS_DIR (
    if exist "%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_BIN=%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
)

where "%CMAKE_BIN%" >nul 2>&1
if %ERRORLEVEL% equ 0 goto :configure_cmake
if exist "%CMAKE_BIN%" goto :configure_cmake

echo [ERROR] CMake was not found in PATH or Visual Studio directories.
echo Please install CMake or ensure it is accessible in PATH.
exit /b 1

:configure_cmake
echo [INFO] CMake tool: %CMAKE_BIN%
echo [INFO] Configuring CMake Project (Release)...
"%CMAKE_BIN%" -B build -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo [INFO] Compiling C++ Executables and Test Suites...
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
echo ===================================================================
exit /b 0
