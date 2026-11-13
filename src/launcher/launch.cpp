#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    wchar_t exePathBuffer[MAX_PATH];
    GetModuleFileNameW(NULL, exePathBuffer, MAX_PATH);
    fs::path currentDir = fs::path(exePathBuffer).parent_path();

    std::vector<fs::path> candidatePaths = {
        currentDir / L"workbench_server.exe",
        currentDir / L"bin" / L"workbench_server.exe",
        currentDir / L"build" / L"Release" / L"workbench_server.exe",
        currentDir / L"build" / L"workbench_server.exe"
    };

    fs::path targetServerExe;
    for (const auto& cand : candidatePaths) {
        if (fs::exists(cand)) {
            targetServerExe = cand;
            break;
        }
    }

    if (targetServerExe.empty()) {
        MessageBoxW(
            NULL,
            L"Could not locate 'workbench_server.exe'.\nPlease ensure the application package was extracted completely.",
            L"Unified Engineering Workbench - Launch Error",
            MB_ICONERROR | MB_OK
        );
        return 1;
    }

    // Prepare startup info
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdLine = L"\"" + targetServerExe.wstring() + L"\"";
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    std::wstring workDir = currentDir.wstring();

    BOOL success = CreateProcessW(
        targetServerExe.c_str(),
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE, // Or 0 for headless background
        NULL,
        workDir.c_str(),
        &si,
        &pi
    );

    if (!success) {
        MessageBoxW(
            NULL,
            L"Failed to launch 'workbench_server.exe'.",
            L"Unified Engineering Workbench - Execution Error",
            MB_ICONERROR | MB_OK
        );
        return 2;
    }

    // Close handles to allow server process to run independently
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return 0;
}
#else
int main() {
    return 0;
}
#endif

