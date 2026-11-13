#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include "nlohmann/json.hpp"

namespace workbench::thermal {

struct ThermalParameters {
    double length{1.0};             // Length of rod L in meters [m]
    double area{1e-4};              // Cross-sectional area A in [m²]
    double conductivity{10.0};      // Thermal conductivity k in [W/(m·K)]
    double heatGeneration{10000.0}; // Volumetric heat source q in [W/m³]
    double tLeft{100.0};            // Dirichlet boundary condition T(0) in [°C]
    double tRight{20.0};            // Dirichlet boundary condition T(L) in [°C]
    uint32_t numElements{10};       // Number of linear 2-node finite elements

    bool validate(std::string& outError) const;
    nlohmann::json toJson() const;
};

struct ThermalNodeResult {
    uint32_t index{0};
    double x{0.0};                  // Coordinate position [m]
    double tFem{0.0};               // Finite element temperature [°C]
    double tExact{0.0};             // Analytical exact temperature [°C]
    double absError{0.0};           // Absolute error |T_fem - T_exact| [°C]

    nlohmann::json toJson() const;
};

struct ThermalMidpointResult {
    uint32_t elementIndex{0};
    double xMid{0.0};               // Midpoint position [m]
    double tFemMid{0.0};            // Linear FE interpolated temperature [°C]
    double tExactMid{0.0};          // Exact analytical temperature at midpoint [°C]
    double absError{0.0};           // Absolute midpoint error [°C]

    nlohmann::json toJson() const;
};

struct ThermalSolveResult {
    bool success{false};
    std::string errorMessage;
    ThermalParameters parameters;
    std::vector<ThermalNodeResult> nodes;
    std::vector<ThermalMidpointResult> midpoints;

    double maxTemperature{0.0};
    double xAtMaxTemperature{0.0};
    double maxMidpointError{0.0};
    double meanMidpointError{0.0};
    double theoreticalMaxMidpointError{0.0};

    nlohmann::json toJson() const;
    std::string toCsv() const;
};

struct MeshConvergenceEntry {
    uint32_t numElements{0};
    uint32_t numNodes{0};
    double elementLength{0.0};      // h = L / N [m]
    double maxMidpointError{0.0};   // Observed max midpoint error [°C]
    double theoreticalError{0.0};   // Theoretical q*h² / (8k) [°C]
    double convergenceRate{0.0};    // Order of convergence alpha: log(E1/E2) / log(h1/h2)

    nlohmann::json toJson() const;
};

struct ConvergenceStudyResult {
    bool success{false};
    std::string errorMessage;
    ThermalParameters parameters;
    std::vector<MeshConvergenceEntry> entries;

    nlohmann::json toJson() const;
    std::string toCsv() const;
};

class ThermalSolver1D {
public:
    static ThermalSolveResult solve(const ThermalParameters& params);
    static ConvergenceStudyResult runConvergenceStudy(const ThermalParameters& baseParams,
                                                      const std::vector<uint32_t>& meshSizes = {10, 20, 40, 80});
    static double exactTemperature(double x, const ThermalParameters& params);

private:
    static bool solveTridiagonal(const std::vector<double>& a,
                                 const std::vector<double>& b,
                                 const std::vector<double>& c,
                                 std::vector<double>& d,
                                 std::vector<double>& x);
};

} // namespace workbench::thermal
