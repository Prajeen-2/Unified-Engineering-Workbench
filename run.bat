@echo off
setlocal
echo Starting Unified Engineering Workbench Backend...
if exist "workbench_server.exe" (
    workbench_server.exe %*
) else if exist "build\bin\Release\workbench_server.exe" (
    build\bin\Release\workbench_server.exe %*
) else if exist "build\bin\workbench_server.exe" (
    build\bin\workbench_server.exe %*
) else (
    echo [ERROR] workbench_server.exe not found. Please run build.bat first!
    exit /b 1
)
