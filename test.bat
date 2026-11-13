@echo off
setlocal enabledelayedexpansion

echo ===================================================================
echo   UNIFIED ENGINEERING WORKBENCH - AUTOMATED TEST RUNNER
echo   PASS SCIENCE WORKS R^&D Internship 2026-2027
echo ===================================================================

where ctest.exe >nul 2>&1
if %ERRORLEVEL% equ 0 goto :run_tests

set "VS_DIR="
set "VSWHERE_DIR=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if not exist "%VSWHERE_DIR%\vswhere.exe" goto :no_ctest

pushd "%VSWHERE_DIR%"
for /f "usebackq tokens=*" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_DIR=%%i"
)
popd

if not defined VS_DIR goto :no_ctest

if exist "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" (
    call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
) else if exist "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
)

:run_tests
where ctest.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] ctest.exe not found. Please run build.bat first or configure PATH.
    exit /b 1
)

echo [INFO] Running CTest Verification Suites (Release)...
ctest.exe --test-dir build -C Release --output-on-failure
exit /b %ERRORLEVEL%

:no_ctest
echo [ERROR] Could not initialize environment for ctest.exe.
exit /b 1
