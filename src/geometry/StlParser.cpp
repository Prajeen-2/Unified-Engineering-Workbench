#include "geometry/StlParser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

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

bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           str.compare(0, prefix.size(), prefix) == 0;
}

} // anonymous namespace

nlohmann::json StlGeometry::toJson(bool includeTriangles) const {
    nlohmann::json j = {
        {"solid_name", solidName},
        {"triangle_count", triangleCount},
        {"vertex_count", vertexCount},
        {"surface_area", surfaceArea},
        {"bounding_box", boundingBox.toJson()}
    };

    if (includeTriangles) {
        nlohmann::json triArray = nlohmann::json::array();
        for (const auto& tri : triangles) {
            triArray.push_back(tri.toJson());
        }
        j["triangles"] = triArray;
    }

    return j;
}

nlohmann::json StlParseResult::toJson() const {
    return nlohmann::json{
        {"success", success},
        {"error_message", errorMessage},
        {"geometry", geometry.toJson(true)}
    };
}

StlParseResult StlParser::parseAsciiString(const std::string& stlContent, size_t maxSizeBytes) {
    StlParseResult result;
    if (stlContent.size() > maxSizeBytes) {
        result.success = false;
        result.errorMessage = "STL content size (" + std::to_string(stlContent.size()) +
                              " bytes) exceeds maximum allowable limit of " +
                              std::to_string(maxSizeBytes) + " bytes (50 MB).";
        return result;
    }

    if (stlContent.empty() || trim(stlContent).empty()) {
        result.success = false;
        result.errorMessage = "File is empty (0 bytes or whitespace only).";
        return result;
    }

    // Binary STL detection: check for null bytes '\0' or non-printable binary control characters
    size_t inspectLen = std::min<size_t>(stlContent.size(), 2048);
    for (size_t i = 0; i < inspectLen; ++i) {
        unsigned char c = static_cast<unsigned char>(stlContent[i]);
        if (c == '\0' || (c < 32 && c != '\t' && c != '\r' && c != '\n')) {
            result.success = false;
            result.errorMessage = "Detected binary STL format (contains binary/null characters). "
                                  "Unified Engineering Workbench requires standard ASCII STL format.";
            return result;
        }
    }

    std::istringstream stream(stlContent);
    std::string line;
    size_t lineNumber = 0;

    bool insideSolid = false;
    bool insideFacet = false;
    bool insideLoop = false;

    Triangle currentTriangle;
    std::vector<Vector3D> currentVertices;

    while (std::getline(stream, line)) {
        lineNumber++;
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        std::string lower = toLower(trimmed);

        if (!insideSolid) {
            if (startsWith(lower, "solid")) {
                insideSolid = true;
                if (trimmed.length() > 5) {
                    result.geometry.solidName = trim(trimmed.substr(5));
                    if (result.geometry.solidName.empty()) {
                        result.geometry.solidName = "DefaultSolid";
                    }
                }
                continue;
            } else {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Expected 'solid <name>' header in ASCII STL.";
                return result;
            }
        }

        if (startsWith(lower, "facet normal")) {
            if (insideFacet) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Nested 'facet' encountered without closing previous facet.";
                return result;
            }
            insideFacet = true;
            currentVertices.clear();

            std::istringstream iss(trimmed.substr(12));
            double nx, ny, nz;
            if (!(iss >> nx >> ny >> nz)) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Failed to parse facet normal vector coordinates.";
                return result;
            }

            if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Non-finite coordinate (NaN/Inf) detected in facet normal.";
                return result;
            }

            currentTriangle.normal = Vector3D(nx, ny, nz);
            continue;
        }

        if (startsWith(lower, "outer loop")) {
            if (!insideFacet || insideLoop) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Unexpected 'outer loop' outside of facet block.";
                return result;
            }
            insideLoop = true;
            continue;
        }

        if (startsWith(lower, "vertex")) {
            if (!insideLoop) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": 'vertex' encountered outside of 'outer loop'.";
                return result;
            }

            std::istringstream iss(trimmed.substr(6));
            double vx, vy, vz;
            if (!(iss >> vx >> vy >> vz)) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Failed to parse vertex coordinates.";
                return result;
            }

            if (!std::isfinite(vx) || !std::isfinite(vy) || !std::isfinite(vz)) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Non-finite coordinate (NaN/Inf) detected in vertex.";
                return result;
            }

            Vector3D vert(vx, vy, vz);
            currentVertices.push_back(vert);
            result.geometry.boundingBox.update(vert);
            continue;
        }

        if (startsWith(lower, "endloop")) {
            if (!insideLoop) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Unexpected 'endloop' without active outer loop.";
                return result;
            }
            if (currentVertices.size() != 3) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Facet must contain exactly 3 vertices, found " +
                                      std::to_string(currentVertices.size()) + ".";
                return result;
            }
            insideLoop = false;
            continue;
        }

        if (startsWith(lower, "endfacet")) {
            if (!insideFacet || insideLoop) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": Unexpected 'endfacet' before loop was closed.";
                return result;
            }

            currentTriangle.v1 = currentVertices[0];
            currentTriangle.v2 = currentVertices[1];
            currentTriangle.v3 = currentVertices[2];

            // Compute exact geometric facet normal and area
            Vector3D edge1 = currentTriangle.v2 - currentTriangle.v1;
            Vector3D edge2 = currentTriangle.v3 - currentTriangle.v1;
            Vector3D crossProd = edge1.cross(edge2);
            double crossNorm = crossProd.length();

            currentTriangle.area = 0.5 * crossNorm;
            result.geometry.surfaceArea += currentTriangle.area;

            // If normal from file was (0,0,0) or invalid, compute it from cross product
            if (currentTriangle.normal.length() < 1e-12 && crossNorm > 1e-12) {
                currentTriangle.normal = crossProd.normalized();
            }

            result.geometry.triangles.push_back(currentTriangle);
            insideFacet = false;
            continue;
        }

        if (startsWith(lower, "endsolid")) {
            if (insideFacet || insideLoop) {
                result.success = false;
                result.errorMessage = "Line " + std::to_string(lineNumber) +
                                      ": 'endsolid' reached while facet or loop was still open.";
                return result;
            }
            insideSolid = false;
            break;
        }
    }

    if (result.geometry.triangles.empty()) {
        result.success = false;
        result.errorMessage = "STL parsing finished with 0 valid triangles.";
        return result;
    }

    result.geometry.triangleCount = result.geometry.triangles.size();
    result.geometry.vertexCount = result.geometry.triangleCount * 3;
    result.geometry.boundingBox.finalize();
    result.success = true;
    return result;
}

StlParseResult StlParser::parseAsciiFile(const std::string& filePath, size_t maxSizeBytes) {
    StlParseResult result;
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
        result.success = false;
        result.errorMessage = "Unable to open STL file: " + filePath + " (file does not exist).";
        return result;
    }

    auto fileSize = std::filesystem::file_size(filePath, ec);
    if (!ec && fileSize > maxSizeBytes) {
        result.success = false;
        result.errorMessage = "STL file size (" + std::to_string(fileSize) +
                              " bytes) exceeds maximum allowable limit of " +
                              std::to_string(maxSizeBytes) + " bytes (50 MB).";
        return result;
    }

    if (!ec && fileSize == 0) {
        result.success = false;
        result.errorMessage = "File is empty (0 bytes or whitespace only).";
        return result;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        result.success = false;
        result.errorMessage = "Unable to open STL file: " + filePath;
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseAsciiString(buffer.str(), maxSizeBytes);
}

} // namespace workbench::geometry

