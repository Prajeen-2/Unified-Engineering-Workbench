#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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

// Check if a local port is free to bind
static bool IsPortFree(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int res = bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    closesocket(sock);
    WSACleanup();

    return (res == 0);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    // 1. If an existing backend server instance is already running on any port in range 8080-8089,
    // open browser to that active instance rather than spawning duplicate
    for (int port = 8080; port <= 8089; ++port) {
        if (IsServerHealthy(port)) {
            std::wstring activeUrl = L"http://127.0.0.1:" + std::to_wstring(port);
            ShellExecuteW(NULL, L"open", activeUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }
    }

    // 2. Select first available port in fallback range 8080-8089
    int selectedPort = 8080;
    bool foundPort = false;
    for (int port = 8080; port <= 8089; ++port) {
        if (IsPortFree(port)) {
            selectedPort = port;
            foundPort = true;
            break;
        }
    }

    if (!foundPort) {
        MessageBoxW(
            NULL,
            L"Unable to bind to any local port in the range 8080-8089.\nPlease close other programs utilizing these ports and try again.",
            L"Unified Engineering Workbench - Port Conflict",
            MB_ICONERROR | MB_OK
        );
        return 4;
    }

    std::wstring appUrl = L"http://127.0.0.1:" + std::to_wstring(selectedPort);

    // 3. Locate workbench_server.exe relative to launcher
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

    // 4. Launch workbench_server.exe with explicit port and hidden window
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdLine = L"\"" + targetServerExe.wstring() + L"\" --port " + 
                           std::to_wstring(selectedPort) + L" --no-browser";
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

    // 5. Poll /api/health until server responds HTTP 200 (timeout: 10 seconds = 50 * 200ms)
    bool serverHealthy = false;
    for (int attempt = 0; attempt < 50; ++attempt) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            break;
        }

        if (IsServerHealthy(selectedPort)) {
            serverHealthy = true;
            break;
        }

        Sleep(200);
    }

    if (!serverHealthy) {
        std::wstring timeoutMsg = L"The backend server process was started on port " + 
                                  std::to_wstring(selectedPort) + 
                                  L", but did not respond to health checks at http://127.0.0.1:" + 
                                  std::to_wstring(selectedPort) + 
                                  L"/api/health within 10 seconds.";
        MessageBoxW(
            NULL,
            timeoutMsg.c_str(),
            L"Unified Engineering Workbench - Startup Timeout",
            MB_ICONERROR | MB_OK
        );
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 3;
    }

    // 6. Open user default web browser to the application URL
    ShellExecuteW(NULL, L"open", appUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);

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
