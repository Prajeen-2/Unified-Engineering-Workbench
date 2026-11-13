@echo off
setlocal enabledelayedexpansion

echo ===================================================================
echo   UNIFIED ENGINEERING WORKBENCH - PACKAGING SYSTEM
echo   PASS SCIENCE WORKS R^&D Internship 2026-2027
echo ===================================================================

set "CANDIDATE_ID=CandidateID"
if not "%~1"=="" set "CANDIDATE_ID=%~1"
set "PACKAGE_NAME=%CANDIDATE_ID%_Workbench.zip"

echo [INFO] Packaging target: %PACKAGE_NAME%

:: 1. Ensure binaries are built
if not exist "Launch.exe" goto :need_build
if not exist "workbench_server.exe" goto :need_build
goto :have_binaries

:need_build
echo [WARN] Binaries not found in root directory. Running build.bat...
call build.bat
if %ERRORLEVEL% neq 0 goto :build_failed

:have_binaries
:: 2. Ensure tar.exe is present (built into Windows 10 1803+ / Windows 11)
where tar.exe >nul 2>&1
if %ERRORLEVEL% neq 0 goto :no_tar

:: 3. Clean any existing package with the same name
if exist "%PACKAGE_NAME%" (
    echo [INFO] Removing previous %PACKAGE_NAME%...
    del /f /q "%PACKAGE_NAME%"
)

:: 4. Assemble package using tar.exe with zip compression (-a)
echo [INFO] Creating ZIP archive with forward-slash paths...
tar.exe -a -c -f "%PACKAGE_NAME%" Launch.exe workbench_server.exe frontend data README.md THIRD_PARTY_NOTICES.md DEPENDENCIES.lock.md
if %ERRORLEVEL% neq 0 goto :pack_failed

echo ===================================================================
echo [SUCCESS] Package created successfully: %PACKAGE_NAME%
echo Archive file list preview:
tar.exe -tf "%PACKAGE_NAME%" | findstr /i "\.exe \.md index\.html"
echo ===================================================================
exit /b 0

:no_tar
echo [ERROR] System tar utility tar.exe was not found in PATH.
exit /b 1

:build_failed
echo [ERROR] Build failed. Cannot create package.
exit /b 1

:pack_failed
echo [ERROR] Failed to create package archive.
exit /b 1
