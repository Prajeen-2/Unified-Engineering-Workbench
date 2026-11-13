#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

namespace fs = std::filesystem;

// Direct Winsock probe to check if http://127.0.0.1:port/api/health answers HTTP 200 OK
static bool IsServerHealthy(int port = 8080) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    DWORD timeoutMs = 500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    sockaddr_in serverAddr;
    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) != 0) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    const char* httpRequest = 
        "GET /api/health HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: close\r\n\r\n";

    send(sock, httpRequest, static_cast<int>(strlen(httpRequest)), 0);

    char buffer[512];
    int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
    closesocket(sock);
    WSACleanup();

    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        return (strstr(buffer, "200 OK") != nullptr);
    }

    return false;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    const int targetPort = 8080;
    const wchar_t* appUrl = L"http://127.0.0.1:8080";

    // 1. If backend server is already running and healthy, open browser and do not spawn duplicate
    if (IsServerHealthy(targetPort)) {
        ShellExecuteW(NULL, L"open", appUrl, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }

    // 2. Locate workbench_server.exe relative to launcher
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
            L"Could not locate 'workbench_server.exe'.\nPlease verify that the application has been built or properly extracted.",
            L"Unified Engineering Workbench - Startup Error",
            MB_ICONERROR | MB_OK
        );
        return 1;
    }

    // 3. Launch workbench_server.exe hidden (CREATE_NO_WINDOW) with detached process group
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdLine = L"\"" + targetServerExe.wstring() + L"\" --no-browser";
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    std::wstring workDir = currentDir.wstring();

    BOOL created = CreateProcessW(
        targetServerExe.c_str(),
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
        NULL,
        workDir.c_str(),
        &si,
        &pi
    );

    if (!created) {
        MessageBoxW(
            NULL,
            L"Failed to launch 'workbench_server.exe'.",
            L"Unified Engineering Workbench - Process Error",
            MB_ICONERROR | MB_OK
        );
        return 2;
    }

    // 4. Poll /api/health until server responds HTTP 200 (timeout: 10 seconds = 50 * 200ms)
    bool serverHealthy = false;
    for (int attempt = 0; attempt < 50; ++attempt) {
        // Check if process unexpectedly exited
        DWORD exitCode = 0;
        if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            break;
        }

        if (IsServerHealthy(targetPort)) {
            serverHealthy = true;
            break;
        }

        Sleep(200);
    }

    if (!serverHealthy) {
        MessageBoxW(
            NULL,
            L"The backend server process was started, but did not respond to health checks at http://127.0.0.1:8080/api/health within 10 seconds.",
            L"Unified Engineering Workbench - Startup Timeout",
            MB_ICONERROR | MB_OK
        );
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 3;
    }

    // 5. Open user default web browser to the application URL
    ShellExecuteW(NULL, L"open", appUrl, NULL, NULL, SW_SHOWNORMAL);

    // Leave server running in background and release process handles
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return 0;
}
#else
int main() {
    return 0;
}
#endif
