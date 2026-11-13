#pragma once

#include <string>
#include <cstdint>
#include "nlohmann/json.hpp"

namespace workbench::core {

struct SystemTelemetry {
    std::string cpuModel;
    uint32_t physicalCores{0};
    uint32_t logicalCores{0};
    uint64_t totalRamBytes{0};
    uint64_t availableRamBytes{0};
    std::string osName;
    std::string compiler;
    std::string buildType;
    std::string architecture;
    uint64_t uptimeSeconds{0};

    nlohmann::json toJson() const;
};

class SystemInfoProvider {
public:
    static SystemTelemetry getTelemetry();
    static std::string getExecutableDir();
    static std::string resolveFrontendPath();

private:
    static std::string queryCpuModel();
    static uint32_t queryPhysicalCores();
};

} // namespace workbench::core
