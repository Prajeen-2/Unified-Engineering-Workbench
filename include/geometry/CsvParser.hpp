#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "geometry/StlParser.hpp"
#include "nlohmann/json.hpp"

namespace workbench::geometry {

struct ThermalNode {
    uint32_t nodeId{0};
    double x{0.0};           // Position in meters [m]
    double y{0.0};           // Position in meters [m]
    double z{0.0};           // Position in meters [m]
    double temperature{0.0}; // Temperature in Celsius [°C]

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"node_id", nodeId},
            {"x", x},
            {"y", y},
            {"z", z},
            {"temperature", temperature}
        };
    }
};

struct ThermalCloud {
    std::vector<ThermalNode> nodes;
    size_t nodeCount{0};
    double minTemperature{0.0};
    double maxTemperature{0.0};
    double meanTemperature{0.0};
    BoundingBox boundingBox;

    nlohmann::json toJson(bool includeNodes = true) const;
};

struct CsvParseResult {
    bool success{false};
    std::string errorMessage;
    ThermalCloud cloud;

    nlohmann::json toJson() const;
};

class CsvParser {
public:
    static CsvParseResult parseString(const std::string& csvContent);
    static CsvParseResult parseFile(const std::string& filePath);
};

} // namespace workbench::geometry

