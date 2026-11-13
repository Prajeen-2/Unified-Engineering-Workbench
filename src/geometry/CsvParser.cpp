#include "geometry/CsvParser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace workbench::geometry {

namespace {

std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream stream(line);
    while (std::getline(stream, token, ',')) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

} // anonymous namespace

nlohmann::json ThermalCloud::toJson(bool includeNodes) const {
    nlohmann::json j = {
        {"node_count", nodeCount},
        {"min_temperature_C", minTemperature},
        {"max_temperature_C", maxTemperature},
        {"mean_temperature_C", meanTemperature},
        {"bounding_box", boundingBox.toJson()}
    };

    if (includeNodes) {
        nlohmann::json nodeArray = nlohmann::json::array();
        for (const auto& node : nodes) {
            nodeArray.push_back(node.toJson());
        }
        j["nodes"] = nodeArray;
    }

    return j;
}

nlohmann::json CsvParseResult::toJson() const {
    return nlohmann::json{
        {"success", success},
        {"error_message", errorMessage},
        {"cloud", cloud.toJson(true)}
    };
}

CsvParseResult CsvParser::parseString(const std::string& csvContent) {
    CsvParseResult result;
    if (csvContent.empty()) {
        result.success = false;
        result.errorMessage = "CSV content is empty.";
        return result;
    }

    std::istringstream stream(csvContent);
    std::string line;
    size_t lineNumber = 0;

    // Read header line
    while (std::getline(stream, line)) {
        lineNumber++;
        std::string trimmed = trim(line);
        if (!trimmed.empty()) break;
    }

    if (line.empty()) {
        result.success = false;
        result.errorMessage = "CSV file has no header row.";
        return result;
    }

    std::vector<std::string> headers = splitCsvLine(line);
    int colNodeId = -1;
    int colX = -1;
    int colY = -1;
    int colZ = -1;
    int colTemp = -1;

    for (size_t i = 0; i < headers.size(); ++i) {
        std::string h = toLower(headers[i]);
        if (h == "node_id" || h == "nodeid" || h == "id") colNodeId = static_cast<int>(i);
        else if (h == "x_m" || h == "x" || h == "x [m]") colX = static_cast<int>(i);
        else if (h == "y_m" || h == "y" || h == "y [m]") colY = static_cast<int>(i);
        else if (h == "z_m" || h == "z" || h == "z [m]") colZ = static_cast<int>(i);
        else if (h == "temperature_c" || h == "temp_c" || h == "temperature" || h == "temp [c]") colTemp = static_cast<int>(i);
    }

    std::vector<std::string> missingColumns;
    if (colNodeId == -1) missingColumns.push_back("node_id");
    if (colX == -1) missingColumns.push_back("x_m");
    if (colY == -1) missingColumns.push_back("y_m");
    if (colZ == -1) missingColumns.push_back("z_m");
    if (colTemp == -1) missingColumns.push_back("temperature_C");

    if (!missingColumns.empty()) {
        result.success = false;
        std::string err = "Missing required column(s): ";
        for (size_t i = 0; i < missingColumns.size(); ++i) {
            if (i > 0) err += ", ";
            err += missingColumns[i];
        }
        result.errorMessage = err;
        return result;
    }

    double tempSum = 0.0;
    double tMin = 1e30;
    double tMax = -1e30;
    std::unordered_set<uint32_t> seenNodeIds;

    while (std::getline(stream, line)) {
        lineNumber++;
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        std::vector<std::string> tokens = splitCsvLine(trimmed);
        int maxIndex = std::max({colNodeId, colX, colY, colZ, colTemp});
        if (static_cast<int>(tokens.size()) <= maxIndex) {
            result.success = false;
            result.errorMessage = "Line " + std::to_string(lineNumber) +
                                  ": Incomplete row (expected at least " +
                                  std::to_string(maxIndex + 1) + " columns, got " +
                                  std::to_string(tokens.size()) + ").";
            return result;
        }

        uint32_t nid = 0;
        double x = 0.0, y = 0.0, z = 0.0, temp = 0.0;

        try {
            nid = static_cast<uint32_t>(std::stoul(tokens[colNodeId]));
            x = std::stod(tokens[colX]);
            y = std::stod(tokens[colY]);
            z = std::stod(tokens[colZ]);
            temp = std::stod(tokens[colTemp]);
        } catch (const std::exception&) {
            result.success = false;
            result.errorMessage = "Line " + std::to_string(lineNumber) +
                                  ": Non-numeric token encountered in numerical field.";
            return result;
        }

        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            result.success = false;
            result.errorMessage = "Line " + std::to_string(lineNumber) +
                                  ": Non-finite coordinate value (NaN or Inf) detected.";
            return result;
        }

        if (!std::isfinite(temp)) {
            result.success = false;
            result.errorMessage = "Line " + std::to_string(lineNumber) +
                                  ": Non-finite temperature value (NaN or Inf) detected.";
            return result;
        }

        if (temp < -273.15) {
            result.success = false;
            result.errorMessage = "Line " + std::to_string(lineNumber) +
                                  ": Temperature " + std::to_string(temp) +
                                  " °C is below absolute zero (-273.15 °C).";
            return result;
        }

        seenNodeIds.insert(nid);

        ThermalNode node;
        node.nodeId = nid;
        node.x = x;
        node.y = y;
        node.z = z;
        node.temperature = temp;

        if (temp < tMin) tMin = temp;
        if (temp > tMax) tMax = temp;
        tempSum += temp;

        result.cloud.boundingBox.update(Vector3D(x, y, z));
        result.cloud.nodes.push_back(node);
    }

    if (result.cloud.nodes.empty()) {
        result.success = false;
        result.errorMessage = "No valid data rows found in CSV.";
        return result;
    }

    result.cloud.nodeCount = result.cloud.nodes.size();
    result.cloud.minTemperature = tMin;
    result.cloud.maxTemperature = tMax;
    result.cloud.meanTemperature = tempSum / static_cast<double>(result.cloud.nodeCount);
    result.cloud.boundingBox.finalize();
    result.success = true;

    return result;
}

CsvParseResult CsvParser::parseFile(const std::string& filePath) {
    CsvParseResult result;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        result.success = false;
        result.errorMessage = "Unable to open CSV file: " + filePath;
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseString(buffer.str());
}

} // namespace workbench::geometry

