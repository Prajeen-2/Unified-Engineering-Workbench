#include "thermal/ThermalSolver1D.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "[TEST FAILED] " << message << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

#define ASSERT_NEAR(val1, val2, tol, message) \
    do { \
        if (std::abs((val1) - (val2)) > (tol)) { \
            std::cerr << "[TEST FAILED] " << message << ": expected " << (val2) \
                      << ", got " << (val1) << " (diff=" << std::abs((val1) - (val2)) \
                      << ", tol=" << (tol) << ")\n"; \
            return false; \
        } \
    } while (0)

bool testBenchmarkSolverCorrectness() {
    workbench::thermal::ThermalParameters params;
    params.length = 1.0;
    params.area = 1e-4;
    params.conductivity = 10.0;
    params.heatGeneration = 10000.0;
    params.tLeft = 100.0;
    params.tRight = 20.0;
    params.numElements = 10;

    auto res = workbench::thermal::ThermalSolver1D::solve(params);
    ASSERT_TRUE(res.success, "Solver must succeed for benchmark parameters");
    ASSERT_TRUE(res.nodes.size() == 11, "N=10 must have 11 nodes");
    ASSERT_TRUE(res.midpoints.size() == 10, "N=10 must have 10 midpoints");

    // Boundary conditions verification
    ASSERT_NEAR(res.nodes[0].tFem, 100.0, 1e-11, "Left boundary condition T(0) must be exactly 100.0 C");
    ASSERT_NEAR(res.nodes[10].tFem, 20.0, 1e-11, "Right boundary condition T(L) must be exactly 20.0 C");

    // Superconvergence at nodes (nodal error should be near machine precision)
    for (const auto& n : res.nodes) {
        ASSERT_NEAR(n.tFem, n.tExact, 1e-9, "Nodal FE temperature must match exact analytical temperature");
    }

    // Midpoint error verification for N=10 (h=0.1): e_mid = q*h²/(8*k) = 10000*0.01/80 = 1.25 C
    ASSERT_NEAR(res.maxMidpointError, 1.250000, 1e-5, "Maximum midpoint error for N=10 must be 1.25 C");
    ASSERT_NEAR(res.theoreticalMaxMidpointError, 1.250000, 1e-5, "Theoretical error for N=10 must be 1.25 C");

    std::cout << "  [PASS] testBenchmarkSolverCorrectness (N=10, Max Midpoint Error: " << res.maxMidpointError << " C)\n";
    return true;
}

bool testMeshConvergenceRate() {
    workbench::thermal::ThermalParameters params;
    params.length = 1.0;
    params.area = 1e-4;
    params.conductivity = 10.0;
    params.heatGeneration = 10000.0;
    params.tLeft = 100.0;
    params.tRight = 20.0;

    std::vector<uint32_t> meshSizes = {10, 20, 40, 80};
    auto study = workbench::thermal::ThermalSolver1D::runConvergenceStudy(params, meshSizes);

    ASSERT_TRUE(study.success, "Convergence study must succeed");
    ASSERT_TRUE(study.entries.size() == 4, "Must have 4 entries for N=10,20,40,80");

    // Verify N=10: Error = 1.250000 C
    ASSERT_NEAR(study.entries[0].maxMidpointError, 1.250000, 1e-5, "N=10 Error mismatch");
    // Verify N=20: Error = 0.312500 C
    ASSERT_NEAR(study.entries[1].maxMidpointError, 0.312500, 1e-5, "N=20 Error mismatch");
    // Verify N=40: Error = 0.078125 C
    ASSERT_NEAR(study.entries[2].maxMidpointError, 0.078125, 1e-6, "N=40 Error mismatch");
    // Verify N=80: Error = 0.019531 C
    ASSERT_NEAR(study.entries[3].maxMidpointError, 0.01953125, 1e-7, "N=80 Error mismatch");

    // Verify quadratic convergence rate alpha ≈ 2.0 for all step halvings
    for (size_t i = 1; i < study.entries.size(); ++i) {
        double rate = study.entries[i].convergenceRate;
        ASSERT_NEAR(rate, 2.000, 0.005, "Order of convergence alpha must be 2.0 (quadratic)");
    }

    std::cout << "  [PASS] testMeshConvergenceRate (Verified exact O(h²) quadratic convergence alpha=2.000)\n";
    return true;
}

bool testMidpointInterpolationAccuracy() {
    workbench::thermal::ThermalParameters params;
    params.length = 1.0;
    params.area = 1e-4;
    params.conductivity = 10.0;
    params.heatGeneration = 10000.0;
    params.tLeft = 100.0;
    params.tRight = 20.0;
    params.numElements = 20;

    auto res = workbench::thermal::ThermalSolver1D::solve(params);
    ASSERT_TRUE(res.success, "Solve must succeed");

    // Midpoint of element 0 is at x = 0.025
    double xMid0 = res.midpoints[0].xMid;
    ASSERT_NEAR(xMid0, 0.025, 1e-12, "First element midpoint must be x=0.025 m");

    // Nodal values: T_0 = 100, T_1 = exact at x=0.05: 100 + 420(0.05) - 500(0.0025) = 119.75 C
    // FE interpolated midpoint: (100 + 119.75)/2 = 109.875 C
    // Exact analytical midpoint at x=0.025: 100 + 420(0.025) - 500(0.000625) = 110.1875 C
    // Difference: 110.1875 - 109.875 = 0.3125 C
    ASSERT_NEAR(res.midpoints[0].tFemMid, 109.875, 1e-6, "Interpolated midpoint temperature mismatch");
    ASSERT_NEAR(res.midpoints[0].tExactMid, 110.1875, 1e-6, "Exact midpoint temperature mismatch");
    ASSERT_NEAR(res.midpoints[0].absError, 0.3125, 1e-6, "Midpoint error mismatch");

    std::cout << "  [PASS] testMidpointInterpolationAccuracy\n";
    return true;
}

bool testInvalidParameterRejection() {
    workbench::thermal::ThermalParameters p;

    // Negative length
    p.length = -1.0;
    std::string err;
    ASSERT_TRUE(!p.validate(err), "Negative length must fail");

    // Zero area
    p.length = 1.0;
    p.area = 0.0;
    ASSERT_TRUE(!p.validate(err), "Zero area must fail");

    // Zero conductivity
    p.area = 1e-4;
    p.conductivity = 0.0;
    ASSERT_TRUE(!p.validate(err), "Zero conductivity must fail");

    // Less than 2 elements
    p.conductivity = 10.0;
    p.numElements = 1;
    ASSERT_TRUE(!p.validate(err), "numElements < 2 must fail");

    // Temperature below absolute zero
    p.numElements = 10;
    p.tLeft = -300.0;
    ASSERT_TRUE(!p.validate(err), "Temperature below -273.15 C must fail");

    // NaN / Inf
    p.tLeft = 100.0;
    p.heatGeneration = std::numeric_limits<double>::quiet_NaN();
    ASSERT_TRUE(!p.validate(err), "NaN heat generation must fail");

    std::cout << "  [PASS] testInvalidParameterRejection\n";
    return true;
}

bool testAssemblyMatchesClosedFormTridiagonal() {
    workbench::thermal::ThermalParameters params;
    params.length = 1.0;
    params.area = 1e-4;
    params.conductivity = 10.0;
    params.heatGeneration = 10000.0;
    params.tLeft = 100.0;
    params.tRight = 20.0;

    std::vector<uint32_t> testMeshes = {10, 20, 40, 80};
    for (uint32_t N : testMeshes) {
        params.numElements = N;
        auto assembled = workbench::thermal::ThermalSolver1D::solve(params);
        auto closedForm = workbench::thermal::ThermalSolver1D::solveClosedFormTridiagonal(params);

        ASSERT_TRUE(assembled.success, "Assembled solver must succeed");
        ASSERT_TRUE(closedForm.success, "Closed-form solver must succeed");
        ASSERT_TRUE(assembled.nodes.size() == closedForm.nodes.size(), "Node count mismatch");

        for (size_t i = 0; i < assembled.nodes.size(); ++i) {
            ASSERT_NEAR(assembled.nodes[i].tFem, closedForm.nodes[i].tFem, 1e-12,
                        "Assembled FEM vs closed-form tridiagonal solution must match to machine precision");
        }

        ASSERT_NEAR(assembled.maxMidpointError, closedForm.maxMidpointError, 1e-12,
                    "Max midpoint error between assembly and closed-form must match to machine precision");
    }

    // Also verify that convergence study entries include full spatial profiles
    auto study = workbench::thermal::ThermalSolver1D::runConvergenceStudy(params, testMeshes);
    ASSERT_TRUE(study.success, "Study must succeed");
    for (const auto& entry : study.entries) {
        ASSERT_TRUE(!entry.spatialProfile.empty(), "Spatial profile must be populated");
        ASSERT_TRUE(entry.spatialProfile.size() == entry.numNodes, "Spatial profile size must equal node count");
        for (const auto& pt : entry.spatialProfile) {
            ASSERT_TRUE(std::isfinite(pt.x), "x coordinate must be finite");
            ASSERT_TRUE(std::isfinite(pt.tFem), "T_fem must be finite");
        }
    }

    std::cout << "  [PASS] testAssemblyMatchesClosedFormTridiagonal (Matched to <1e-12 C across N=10,20,40,80)\n";
    return true;
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "   RUNNING TASK C 1D THERMAL FEM SOLVER UNIT TESTS      \n";
    std::cout << "========================================================\n";

    bool allPassed = true;
    allPassed &= testBenchmarkSolverCorrectness();
    allPassed &= testMeshConvergenceRate();
    allPassed &= testMidpointInterpolationAccuracy();
    allPassed &= testInvalidParameterRejection();
    allPassed &= testAssemblyMatchesClosedFormTridiagonal();

    std::cout << "========================================================\n";
    if (allPassed) {
        std::cout << "   ALL TASK C THERMAL FEM UNIT TESTS PASSED (5/5)      \n";
        std::cout << "========================================================\n";
        return 0;
    } else {
        std::cerr << "   SOME THERMAL TESTS FAILED!                           \n";
        std::cout << "========================================================\n";
        return 1;
    }
}

