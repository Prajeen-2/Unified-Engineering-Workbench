#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "thermal/ThermalSolver1D.hpp"
#include "performance/ThreadPool.hpp"
#include "nlohmann/json.hpp"

namespace workbench::performance {

struct BenchmarkConfig {
    size_t numCases{500};                         // Number of independent simulation cases
    uint32_t numElements{100};                    // Elements per rod in FEM discretization
    uint32_t randomSeed{42};                      // Deterministic PRNG seed
    std::vector<size_t> workerCounts{1, 2, 3, 4}; // Worker pool sizes to benchmark
    size_t repetitions{5};                        // Number of measured repetitions per worker count (5 as required)

    bool validate(std::string& outError) const;
    nlohmann::json toJson() const;
    static BenchmarkConfig fromJson(const nlohmann::json& j);
};

struct WorkerRunResult {
    size_t workerCount{1};
    std::vector<double> repetitionTimesMs;        // 5 measured repetition timings in milliseconds
    double medianTimeMs{0.0};                     // Median execution time
    double meanTimeMs{0.0};                       // Mean execution time
    double minTimeMs{0.0};                        // Minimum execution time
    double maxTimeMs{0.0};                        // Maximum execution time
    double stdDevTimeMs{0.0};                     // Standard deviation of execution times
    double speedup{1.0};                          // Speedup metric: S_p = T_1 / T_p
    double efficiency{1.0};                       // Parallel efficiency: E_p = S_p / p
    double maxTempDiffVsSerial{0.0};              // Maximum absolute nodal temperature discrepancy vs serial [°C]
    bool verified{true};                          // Strict verification against 1e-8 °C tolerance

    nlohmann::json toJson() const;
};

struct BenchmarkSuiteResult {
    bool success{false};
    std::string errorMessage;
    BenchmarkConfig config;
    double launchToReadyMs{0.0};                  // Server launch-to-ready initialization time [ms]
    double serialBaselineMedianMs{0.0};           // Median execution time of serial baseline (T1) [ms]
    std::vector<WorkerRunResult> workerResults;   // Results for each evaluated worker count
    bool allVerified{true};                       // True if every worker configuration satisfies <= 1e-8 °C
    std::string timestamp;                        // ISO-formatted execution timestamp

    nlohmann::json toJson() const;
    std::string toCsv() const;
};

class PerformanceBenchmark {
public:
    // Generate deterministic thermal FEA workload batch
    static std::vector<workbench::thermal::ThermalParameters> generateWorkload(
        size_t numCases, uint32_t numElements, uint32_t seed = 42);

    // Run serial execution across all cases
    static std::vector<workbench::thermal::ThermalSolveResult> runSerial(
        const std::vector<workbench::thermal::ThermalParameters>& cases);

    // Run parallel execution using pre-created ThreadPool (measures computation only)
    static void runParallel(
        const std::vector<workbench::thermal::ThermalParameters>& cases,
        ThreadPool& pool,
        std::vector<workbench::thermal::ThermalSolveResult>& outResults);

    // Run parallel execution across bounded worker pool preserving deterministic ordering
    static std::vector<workbench::thermal::ThermalSolveResult> runParallel(
        const std::vector<workbench::thermal::ThermalParameters>& cases, size_t numWorkers);

    // Point-by-point temperature comparison verifying max discrepancy <= 1e-8 °C
    static double compareResults(
        const std::vector<workbench::thermal::ThermalSolveResult>& resA,
        const std::vector<workbench::thermal::ThermalSolveResult>& resB);

    // Execute full benchmark suite with warm-up, 5 repetitions, and metrics
    static BenchmarkSuiteResult runSuite(
        const BenchmarkConfig& config, double launchToReadyMs = 0.0);
};

} // namespace workbench::performance
