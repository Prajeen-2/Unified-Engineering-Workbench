#include "core/SystemInfo.hpp"
#include <chrono>
#include <thread>
#include <filesystem>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#endif

namespace workbench::core {

namespace {
    static const auto s_startTime = std::chrono::steady_clock::now();
}

nlohmann::json SystemTelemetry::toJson() const {
    double totalRamGB = static_cast<double>(totalRamBytes) / (1024.0 * 1024.0 * 1024.0);
    double availRamGB = static_cast<double>(availableRamBytes) / (1024.0 * 1024.0 * 1024.0);

    return nlohmann::json{
        {"cpu_model", cpuModel},
        {"physical_cores", physicalCores},
        {"logical_cores", logicalCores},
        {"total_ram_bytes", totalRamBytes},
        {"available_ram_bytes", availableRamBytes},
        {"total_ram_gb", std::round(totalRamGB * 100.0) / 100.0},
        {"available_ram_gb", std::round(availRamGB * 100.0) / 100.0},
        {"os_name", osName},
        {"compiler", compiler},
        {"build_type", buildType},
        {"architecture", architecture},
        {"uptime_seconds", uptimeSeconds}
    };
}

std::string SystemInfoProvider::queryCpuModel() {
#if defined(_WIN32)
    int cpuInfo[4] = {-1};
    char brand[0x40] = {0};

    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = cpuInfo[0];

    if (nExIds >= 0x80000004) {
        __cpuid(reinterpret_cast<int*>(brand), 0x80000002);
        __cpuid(reinterpret_cast<int*>(brand + 16), 0x80000003);
        __cpuid(reinterpret_cast<int*>(brand + 32), 0x80000004);
        
        // Trim leading spaces if any
        char* p = brand;
        while (*p == ' ') p++;
        return std::string(p);
    }
#endif
    return "Generic x64 Processor";
}

uint32_t SystemInfoProvider::queryPhysicalCores() {
#if defined(_WIN32)
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<uint8_t> buffer(length);
        auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length)) {
            uint32_t count = 0;
            DWORD offset = 0;
            while (offset < length) {
                auto current = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
                if (current->Relationship == RelationProcessorCore) {
                    count++;
                }
                offset += current->Size;
            }
            if (count > 0) return count;
        }
    }
#endif
    uint32_t logical = std::thread::hardware_concurrency();
    return (logical > 1) ? (logical / 2) : 1;
}

SystemTelemetry SystemInfoProvider::getTelemetry() {
    SystemTelemetry t;
    t.cpuModel = queryCpuModel();
    t.logicalCores = std::thread::hardware_concurrency();
    t.physicalCores = queryPhysicalCores();

#if defined(_WIN32)
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        t.totalRamBytes = memStatus.ullTotalPhys;
        t.availableRamBytes = memStatus.ullAvailPhys;
    }
    t.osName = "Windows 11 x64";
#else
    t.osName = "POSIX / Other";
#endif

#if defined(_MSC_VER)
    std::ostringstream ssComp;
    ssComp << "MSVC " << _MSC_VER << " (Visual Studio 2026)";
    t.compiler = ssComp.str();
#elif defined(__clang__)
    t.compiler = std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
    t.compiler = std::string("GCC ") + __VERSION__;
#else
    t.compiler = "Standard C++17 Compiler";
#endif

#if defined(NDEBUG)
    t.buildType = "Release (Optimized /O2)";
#else
    t.buildType = "Debug";
#endif

    t.architecture = "x86_64 / x64";

    auto now = std::chrono::steady_clock::now();
    t.uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - s_startTime).count();

    return t;
}

std::string SystemInfoProvider::getExecutableDir() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::filesystem::path p(buffer);
    return p.parent_path().string();
#else
    return std::filesystem::current_path().string();
#endif
}

std::string SystemInfoProvider::resolvePath(const std::string& relativePath) {
    namespace fs = std::filesystem;
    fs::path exeDir(getExecutableDir());
    fs::path rel(relativePath);

    std::vector<fs::path> candidates = {
        exeDir / rel,
        exeDir.parent_path() / rel,
        exeDir.parent_path().parent_path() / rel,
        fs::current_path() / rel
    };

    for (const auto& cand : candidates) {
        if (fs::exists(cand)) {
            return cand.lexically_normal().string();
        }
    }

    return (exeDir / rel).string();
}

std::string SystemInfoProvider::resolveFrontendPath() {
    return resolvePath("frontend");
}

} // namespace workbench::core

