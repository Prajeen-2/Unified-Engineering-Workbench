#pragma once

#include <string>
#include <vector>
#include <cmath>
#include "nlohmann/json.hpp"

namespace workbench::geometry {

struct Vector3D {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Vector3D() = default;
    Vector3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vector3D operator-(const Vector3D& other) const {
        return Vector3D(x - other.x, y - other.y, z - other.z);
    }

    Vector3D operator+(const Vector3D& other) const {
        return Vector3D(x + other.x, y + other.y, z + other.z);
    }

    Vector3D operator*(double scalar) const {
        return Vector3D(x * scalar, y * scalar, z * scalar);
    }

    Vector3D cross(const Vector3D& other) const {
        return Vector3D(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vector3D normalized() const {
        double len = length();
        if (len > 1e-14) {
            return Vector3D(x / len, y / len, z / len);
        }
        return Vector3D(0.0, 0.0, 0.0);
    }

    nlohmann::json toJson() const {
        return nlohmann::json{{"x", x}, {"y", y}, {"z", z}};
    }
};

struct Triangle {
    Vector3D normal;
    Vector3D v1;
    Vector3D v2;
    Vector3D v3;
    double area{0.0};

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"normal", normal.toJson()},
            {"v1", v1.toJson()},
            {"v2", v2.toJson()},
            {"v3", v3.toJson()},
            {"area", area}
        };
    }
};

struct BoundingBox {
    Vector3D minPoint{1e30, 1e30, 1e30};
    Vector3D maxPoint{-1e30, -1e30, -1e30};
    Vector3D center{0.0, 0.0, 0.0};
    Vector3D dimensions{0.0, 0.0, 0.0};
    double diagonal{0.0};

    void update(const Vector3D& v) {
        if (v.x < minPoint.x) minPoint.x = v.x;
        if (v.y < minPoint.y) minPoint.y = v.y;
        if (v.z < minPoint.z) minPoint.z = v.z;

        if (v.x > maxPoint.x) maxPoint.x = v.x;
        if (v.y > maxPoint.y) maxPoint.y = v.y;
        if (v.z > maxPoint.z) maxPoint.z = v.z;
    }

    void finalize() {
        center = Vector3D(
            0.5 * (minPoint.x + maxPoint.x),
            0.5 * (minPoint.y + maxPoint.y),
            0.5 * (minPoint.z + maxPoint.z)
        );
        dimensions = Vector3D(
            maxPoint.x - minPoint.x,
            maxPoint.y - minPoint.y,
            maxPoint.z - minPoint.z
        );
        diagonal = dimensions.length();
    }

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"min", minPoint.toJson()},
            {"max", maxPoint.toJson()},
            {"center", center.toJson()},
            {"dimensions", dimensions.toJson()},
            {"diagonal", diagonal}
        };
    }
};

struct StlGeometry {
    std::string solidName{"UnnamedSolid"};
    std::vector<Triangle> triangles;
    BoundingBox boundingBox;
    double surfaceArea{0.0};
    size_t vertexCount{0};
    size_t triangleCount{0};

    nlohmann::json toJson(bool includeTriangles = true) const;
};

struct StlParseResult {
    bool success{false};
    std::string errorMessage;
    StlGeometry geometry;

    nlohmann::json toJson() const;
};

class StlParser {
public:
    static StlParseResult parseAsciiString(const std::string& stlContent);
    static StlParseResult parseAsciiFile(const std::string& filePath);
};

} // namespace workbench::geometry
