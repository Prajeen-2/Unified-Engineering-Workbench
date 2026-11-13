#include "thermal/ThermalSolver1D.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace workbench::thermal {

bool ThermalParameters::validate(std::string& outError) const {
    if (!std::isfinite(length) || length <= 1e-6) {
        outError = "Rod length L must be positive and finite (got " + std::to_string(length) + ").";
        return false;
    }
    if (!std::isfinite(area) || area <= 1e-12) {
        outError = "Cross-sectional area A must be positive and finite (got " + std::to_string(area) + ").";
        return false;
    }
    if (!std::isfinite(conductivity) || conductivity <= 1e-6) {
        outError = "Thermal conductivity k must be positive and finite (got " + std::to_string(conductivity) + ").";
        return false;
    }
    if (!std::isfinite(heatGeneration)) {
        outError = "Volumetric heat generation q must be finite (got " + std::to_string(heatGeneration) + ").";
        return false;
    }
    if (!std::isfinite(tLeft) || !std::isfinite(tRight)) {
        outError = "Boundary temperatures T(0) and T(L) must be finite.";
        return false;
    }
    if (tLeft < -273.15 || tRight < -273.15) {
        outError = "Boundary temperatures cannot be below absolute zero (-273.15 °C).";
        return false;
    }
    if (numElements < 2 || numElements > 10000) {
        outError = "Number of finite elements must be between 2 and 10,000 (got " + std::to_string(numElements) + ").";
        return false;
    }
    return true;
}

nlohmann::json ThermalParameters::toJson() const {
    return nlohmann::json{
        {"length_m", length},
        {"area_m2", area},
        {"conductivity_W_mK", conductivity},
        {"heat_generation_W_m3", heatGeneration},
        {"t_left_C", tLeft},
        {"t_right_C", tRight},
        {"num_elements", numElements}
    };
}

nlohmann::json ThermalNodeResult::toJson() const {
    return nlohmann::json{
        {"node_id", index},
        {"x_m", x},
        {"temperature_fem_C", tFem},
        {"temperature_exact_C", tExact},
        {"absolute_error_C", absError}
    };
}

nlohmann::json ThermalMidpointResult::toJson() const {
    return nlohmann::json{
        {"element_id", elementIndex},
        {"x_mid_m", xMid},
        {"temperature_fem_mid_C", tFemMid},
        {"temperature_exact_mid_C", tExactMid},
        {"absolute_error_C", absError}
    };
}

nlohmann::json ThermalSolveResult::toJson() const {
    nlohmann::json nodeArr = nlohmann::json::array();
    for (const auto& n : nodes) nodeArr.push_back(n.toJson());

    nlohmann::json midArr = nlohmann::json::array();
    for (const auto& m : midpoints) midArr.push_back(m.toJson());

    return nlohmann::json{
        {"success", success},
        {"error_message", errorMessage},
        {"parameters", parameters.toJson()},
        {"num_elements", parameters.numElements},
        {"num_nodes", nodes.size()},
        {"max_temperature_C", maxTemperature},
        {"x_at_max_temperature_m", xAtMaxTemperature},
        {"max_midpoint_error_C", maxMidpointError},
        {"mean_midpoint_error_C", meanMidpointError},
        {"theoretical_max_midpoint_error_C", theoreticalMaxMidpointError},
        {"nodes", nodeArr},
        {"midpoints", midArr}
    };
}

std::string ThermalSolveResult::toCsv() const {
    std::ostringstream ss;
    ss << "# Unified Engineering Workbench - 1D Thermal FEA Simulation Results\n";
    ss << "# L=" << parameters.length << "m, A=" << parameters.area << "m2, k=" << parameters.conductivity
       << "W/mK, q=" << parameters.heatGeneration << "W/m3, T(0)=" << parameters.tLeft << "C, T(L)=" << parameters.tRight
       << "C, Elements=" << parameters.numElements << "\n";
    ss << "type,id,x_m,t_fem_C,t_exact_C,abs_error_C\n";

    for (const auto& n : nodes) {
        ss << "node," << n.index << "," << std::fixed << std::setprecision(6)
           << n.x << "," << n.tFem << "," << n.tExact << "," << n.absError << "\n";
    }
    for (const auto& m : midpoints) {
        ss << "midpoint," << m.elementIndex << "," << std::fixed << std::setprecision(6)
           << m.xMid << "," << m.tFemMid << "," << m.tExactMid << "," << m.absError << "\n";
    }
    return ss.str();
}

nlohmann::json MeshConvergenceEntry::toJson() const {
    return nlohmann::json{
        {"num_elements", numElements},
        {"num_nodes", numNodes},
        {"element_length_m", elementLength},
        {"max_midpoint_error_C", maxMidpointError},
        {"theoretical_error_C", theoreticalError},
        {"convergence_rate", convergenceRate}
    };
}

nlohmann::json ConvergenceStudyResult::toJson() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : entries) arr.push_back(e.toJson());

    return nlohmann::json{
        {"success", success},
        {"error_message", errorMessage},
        {"parameters", parameters.toJson()},
        {"study_entries", arr}
    };
}

std::string ConvergenceStudyResult::toCsv() const {
    std::ostringstream ss;
    ss << "# Unified Engineering Workbench - Mesh Convergence Study\n";
    ss << "# Elements,Nodes,h_m,Max_Midpoint_Error_C,Theoretical_Error_C,Convergence_Rate_Alpha\n";
    ss << "elements,nodes,h_m,max_midpoint_error_C,theoretical_error_C,convergence_rate\n";
    for (const auto& e : entries) {
        ss << e.numElements << "," << e.numNodes << ","
           << std::scientific << std::setprecision(6) << e.elementLength << ","
           << e.maxMidpointError << "," << e.theoreticalError << ","
           << std::fixed << std::setprecision(4) << e.convergenceRate << "\n";
    }
    return ss.str();
}

double ThermalSolver1D::exactTemperature(double x, const ThermalParameters& params) {
    // Analytical solution: T_exact(x) = T_0 + (T_L - T_0)*x/L + [q / (2k)] * x * (L - x)
    double linear = params.tLeft + (params.tRight - params.tLeft) * (x / params.length);
    double quadratic = (params.heatGeneration / (2.0 * params.conductivity)) * x * (params.length - x);
    return linear + quadratic;
}

bool ThermalSolver1D::solveTridiagonal(const std::vector<double>& a,
                                      const std::vector<double>& b,
                                      const std::vector<double>& c,
                                      std::vector<double>& d,
                                      std::vector<double>& x) {
    // Thomas Algorithm (TDMA) for Tridiagonal Matrix:
    // a: subdiagonal (size M, a[0] unused)
    // b: main diagonal (size M)
    // c: superdiagonal (size M, c[M-1] unused)
    // d: right-hand side vector (size M)
    // x: solution vector (size M)
    size_t M = b.size();
    if (M == 0) return false;
    if (M == 1) {
        if (std::abs(b[0]) < 1e-15) return false;
        x.resize(1);
        x[0] = d[0] / b[0];
        return true;
    }

    std::vector<double> cPrime(M, 0.0);
    std::vector<double> dPrime(M, 0.0);

    // Forward sweep
    if (std::abs(b[0]) < 1e-15) return false;
    cPrime[0] = c[0] / b[0];
    dPrime[0] = d[0] / b[0];

    for (size_t i = 1; i < M; ++i) {
        double denom = b[i] - a[i] * cPrime[i - 1];
        if (std::abs(denom) < 1e-15) return false;
        if (i < M - 1) {
            cPrime[i] = c[i] / denom;
        }
        dPrime[i] = (d[i] - a[i] * dPrime[i - 1]) / denom;
    }

    // Back substitution
    x.resize(M);
    x[M - 1] = dPrime[M - 1];
    for (int i = static_cast<int>(M) - 2; i >= 0; --i) {
        x[i] = dPrime[i] - cPrime[i] * x[i + 1];
    }

    return true;
}

ThermalSolveResult ThermalSolver1D::solve(const ThermalParameters& params) {
    ThermalSolveResult result;
    result.parameters = params;

    std::string valErr;
    if (!params.validate(valErr)) {
        result.success = false;
        result.errorMessage = valErr;
        return result;
    }

    uint32_t N = params.numElements;
    uint32_t numNodes = N + 1;
    double L = params.length;
    double h = L / static_cast<double>(N);
    double k = params.conductivity;
    double A = params.area;
    double q = params.heatGeneration;

    // Element stiffness coefficient: ke = k * A / h
    double ke = (k * A) / h;
    // Element load contribution: fe = q * A * h / 2
    double fe = (q * A * h) / 2.0;

    // We have N+1 nodes total.
    // Dirichlet boundary conditions at node 0 (x=0) and node N (x=L):
    // T_0 = tLeft, T_N = tRight.
    // Interior system solves for unknown temperatures T_1, T_2, ..., T_{N-1}.
    // Size of interior system: M = N - 1.
    size_t M = N - 1;

    std::vector<double> a(M, -ke);     // subdiagonal
    std::vector<double> b(M, 2.0 * ke); // main diagonal
    std::vector<double> c(M, -ke);     // superdiagonal
    std::vector<double> d(M, 2.0 * fe); // interior load = fe + fe = q * A * h

    // Apply Dirichlet boundary contributions to RHS
    d[0] += ke * params.tLeft;
    d[M - 1] += ke * params.tRight;

    std::vector<double> tInterior;
    if (!solveTridiagonal(a, b, c, d, tInterior)) {
        result.success = false;
        result.errorMessage = "Failed to solve tridiagonal linear finite element system (singular matrix).";
        return result;
    }

    // Assemble complete nodal temperature vector [T_0, T_1, ..., T_N]
    std::vector<double> tNodes(numNodes);
    tNodes[0] = params.tLeft;
    for (size_t i = 0; i < M; ++i) {
        tNodes[i + 1] = tInterior[i];
    }
    tNodes[N] = params.tRight;

    // Populate nodal results & find max temperature
    result.nodes.resize(numNodes);
    double maxT = -1e30;
    double xMaxT = 0.0;

    for (uint32_t i = 0; i < numNodes; ++i) {
        double x = i * h;
        double tFem = tNodes[i];
        double tEx = exactTemperature(x, params);
        double err = std::abs(tFem - tEx);

        result.nodes[i] = {i, x, tFem, tEx, err};

        if (tFem > maxT) {
            maxT = tFem;
            xMaxT = x;
        }
    }
    result.maxTemperature = maxT;
    result.xAtMaxTemperature = xMaxT;

    // Calculate temperatures at element midpoints using linear FE interpolation:
    // T_fem_mid(e) = 0.5 * (T_e + T_{e+1})
    result.midpoints.resize(N);
    double maxMidErr = 0.0;
    double sumMidErr = 0.0;

    for (uint32_t e = 0; e < N; ++e) {
        double xMid = (e + 0.5) * h;
        double tFemMid = 0.5 * (tNodes[e] + tNodes[e + 1]);
        double tExactMid = exactTemperature(xMid, params);
        double err = std::abs(tFemMid - tExactMid);

        result.midpoints[e] = {e, xMid, tFemMid, tExactMid, err};

        if (err > maxMidErr) {
            maxMidErr = err;
        }
        sumMidErr += err;
    }

    result.maxMidpointError = maxMidErr;
    result.meanMidpointError = sumMidErr / static_cast<double>(N);

    // Exact theoretical midpoint error for 1D linear FE with uniform heat generation:
    // e_mid,theoretical = q * h² / (8 * k)
    result.theoreticalMaxMidpointError = (q * h * h) / (8.0 * k);

    result.success = true;
    return result;
}

ConvergenceStudyResult ThermalSolver1D::runConvergenceStudy(const ThermalParameters& baseParams,
                                                            const std::vector<uint32_t>& meshSizes) {
    ConvergenceStudyResult studyResult;
    studyResult.parameters = baseParams;

    std::vector<MeshConvergenceEntry> entries;
    double prevError = 0.0;
    double prevH = 0.0;

    for (size_t i = 0; i < meshSizes.size(); ++i) {
        ThermalParameters p = baseParams;
        p.numElements = meshSizes[i];

        auto sol = solve(p);
        if (!sol.success) {
            studyResult.success = false;
            studyResult.errorMessage = "Convergence study failed on mesh N=" + std::to_string(p.numElements) + ": " + sol.errorMessage;
            return studyResult;
        }

        double currentH = p.length / static_cast<double>(p.numElements);
        double currentError = sol.maxMidpointError;
        double theoretical = sol.theoreticalMaxMidpointError;
        double rate = 0.0;

        if (i > 0 && prevError > 1e-14 && currentError > 1e-14 && prevH > currentH) {
            // Observed order of convergence alpha:
            // alpha = log(E_coarse / E_fine) / log(h_coarse / h_fine)
            rate = std::log(prevError / currentError) / std::log(prevH / currentH);
        }

        entries.push_back({
            p.numElements,
            p.numElements + 1,
            currentH,
            currentError,
            theoretical,
            rate
        });

        prevError = currentError;
        prevH = currentH;
    }

    studyResult.entries = entries;
    studyResult.success = true;
    return studyResult;
}

} // namespace workbench::thermal

