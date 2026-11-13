#include "performance/PerformanceBenchmark.hpp"
#include <random>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

namespace workbench::performance {

bool BenchmarkConfig::validate(std::string& outError) const {
    if (numCases == 0 || numCases > 100000) {
        outError = "Simulation case count must be between 1 and 100,000.";
        return false;
    }
    if (numElements < 2 || numElements > 5000) {
        outError = "Number of mesh elements must be between 2 and 5,000.";
        return false;
    }
    if (repetitions < 1 || repetitions > 50) {
        outError = "Benchmark repetitions must be between 1 and 50.";
        return false;
    }
    if (workerCounts.empty()) {
        outError = "At least one worker count configuration must be specified.";
        return false;
    }
    for (auto w : workerCounts) {
        if (w == 0) {
            outError = "Worker count must be greater than zero.";
            return false;
        }
    }
    return true;
}

nlohmann::json BenchmarkConfig::toJson() const {
    return nlohmann::json{
        {"num_cases", numCases},
        {"num_elements", numElements},
        {"random_seed", randomSeed},
        {"worker_counts", workerCounts},
        {"repetitions", repetitions}
    };
}

BenchmarkConfig BenchmarkConfig::fromJson(const nlohmann::json& j) {
    BenchmarkConfig config;
    if (j.contains("num_cases")) config.numCases = j["num_cases"].get<size_t>();
    if (j.contains("num_elements")) config.numElements = j["num_elements"].get<uint32_t>();
    if (j.contains("random_seed")) config.randomSeed = j["random_seed"].get<uint32_t>();
    if (j.contains("repetitions")) config.repetitions = j["repetitions"].get<size_t>();
    if (j.contains("worker_counts") && j["worker_counts"].is_array()) {
        config.workerCounts.clear();
        for (const auto& w : j["worker_counts"]) {
            config.workerCounts.push_back(w.get<size_t>());
        }
    }
    return config;
}

nlohmann::json WorkerRunResult::toJson() const {
    return nlohmann::json{
        {"worker_count", workerCount},
        {"repetition_times_ms", repetitionTimesMs},
        {"median_time_ms", std::round(medianTimeMs * 10000.0) / 10000.0},
        {"mean_time_ms", std::round(meanTimeMs * 10000.0) / 10000.0},
        {"min_time_ms", std::round(minTimeMs * 10000.0) / 10000.0},
        {"max_time_ms", std::round(maxTimeMs * 10000.0) / 10000.0},
        {"std_dev_time_ms", std::round(stdDevTimeMs * 10000.0) / 10000.0},
        {"speedup", std::round(speedup * 10000.0) / 10000.0},
        {"efficiency", std::round(efficiency * 10000.0) / 10000.0},
        {"max_temp_diff_vs_serial", maxTempDiffVsSerial},
        {"verified", verified}
    };
}

nlohmann::json BenchmarkSuiteResult::toJson() const {
    nlohmann::json jWorkers = nlohmann::json::array();
    for (const auto& w : workerResults) {
        jWorkers.push_back(w.toJson());
    }

    return nlohmann::json{
        {"success", success},
        {"error_message", errorMessage},
        {"config", config.toJson()},
        {"launch_to_ready_ms", std::round(launchToReadyMs * 100.0) / 100.0},
        {"serial_baseline_median_ms", std::round(serialBaselineMedianMs * 10000.0) / 10000.0},
        {"worker_results", jWorkers},
        {"all_verified", allVerified},
        {"timestamp", timestamp}
    };
}

std::string BenchmarkSuiteResult::toCsv() const {
    std::ostringstream ss;
    ss << "# Unified Engineering Workbench - Multicore Performance Lab Benchmark Export\n";
    ss << "# Timestamp: " << timestamp << "\n";
    ss << "# Simulation Cases: " << config.numCases << ", Mesh Elements per Rod: " << config.numElements << "\n";
    ss << "# PRNG Seed: " << config.randomSeed << ", Repetitions: " << config.repetitions << "\n";
    ss << "# Verification Tolerance: 1.000000e-08 °C\n";
    ss << "# Launch-to-Ready Time: " << launchToReadyMs << " ms\n";
    ss << "# Serial Baseline Median (T1): " << serialBaselineMedianMs << " ms\n";
    ss << "workers,run1_ms,run2_ms,run3_ms,run4_ms,run5_ms,median_ms,mean_ms,min_ms,max_ms,stddev_ms,speedup,efficiency,max_temp_diff_C,verification_status\n";
    
    for (const auto& w : workerResults) {
        ss << w.workerCount << ",";
        for (size_t r = 0; r < 5; ++r) {
            if (r < w.repetitionTimesMs.size()) {
                ss << std::fixed << std::setprecision(4) << w.repetitionTimesMs[r];
            } else {
                ss << "0.0000";
            }
            ss << ",";
        }
        ss << std::fixed << std::setprecision(4) << w.medianTimeMs << ","
           << std::fixed << std::setprecision(4) << w.meanTimeMs << ","
           << std::fixed << std::setprecision(4) << w.minTimeMs << ","
           << std::fixed << std::setprecision(4) << w.maxTimeMs << ","
           << std::fixed << std::setprecision(4) << w.stdDevTimeMs << ","
           << std::fixed << std::setprecision(4) << w.speedup << ","
           << std::fixed << std::setprecision(4) << w.efficiency << ","
           << std::scientific << std::setprecision(6) << w.maxTempDiffVsSerial << ","
           << (w.verified ? "PASS" : "FAIL") << "\n";
    }
    return ss.str();
}

std::vector<workbench::thermal::ThermalParameters> PerformanceBenchmark::generateWorkload(
    size_t numCases, uint32_t numElements, uint32_t seed) {
    
    std::vector<workbench::thermal::ThermalParameters> workload;
    workload.reserve(numCases);

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> distK(5.0, 50.0);        // Conductivity [W/(m·K)]
    std::uniform_real_distribution<double> distQ(1000.0, 30000.0);   // Heat generation [W/m³]
    std::uniform_real_distribution<double> distT0(50.0, 150.0);      // Boundary T(0) [°C]
    std::uniform_real_distribution<double> distTL(10.0, 50.0);       // Boundary T(L) [°C]
    std::uniform_real_distribution<double> distL(0.5, 2.0);          // Rod length [m]
    std::uniform_real_distribution<double> distA(0.5e-4, 5.0e-4);    // Area [m²]

    for (size_t i = 0; i < numCases; ++i) {
        workbench::thermal::ThermalParameters p;
        p.length = distL(rng);
        p.area = distA(rng);
        p.conductivity = distK(rng);
        p.heatGeneration = distQ(rng);
        p.tLeft = distT0(rng);
        p.tRight = distTL(rng);
        p.numElements = numElements;
        workload.push_back(p);
    }

    return workload;
}

std::vector<workbench::thermal::ThermalSolveResult> PerformanceBenchmark::runSerial(
    const std::vector<workbench::thermal::ThermalParameters>& cases) {
    
    std::vector<workbench::thermal::ThermalSolveResult> results(cases.size());
    for (size_t i = 0; i < cases.size(); ++i) {
        results[i] = workbench::thermal::ThermalSolver1D::solve(cases[i]);
    }
    return results;
}

void PerformanceBenchmark::runParallel(
    const std::vector<workbench::thermal::ThermalParameters>& cases,
    ThreadPool& pool,
    std::vector<workbench::thermal::ThermalSolveResult>& outResults) {
    
    if (outResults.size() != cases.size()) {
        outResults.resize(cases.size());
    }

    size_t numWorkers = pool.getThreadCount();
    // Dynamic chunking to balance load while keeping queue lock overhead minimal
    const size_t numChunks = std::max<size_t>(numWorkers * 8, 1);
    const size_t chunkSize = (cases.size() + numChunks - 1) / numChunks;

    for (size_t start = 0; start < cases.size(); start += chunkSize) {
        size_t end = std::min(start + chunkSize, cases.size());
        pool.enqueue([&cases, &outResults, start, end]() {
            for (size_t i = start; i < end; ++i) {
                outResults[i] = workbench::thermal::ThermalSolver1D::solve(cases[i]);
            }
        });
    }

    pool.waitAll();
}

std::vector<workbench::thermal::ThermalSolveResult> PerformanceBenchmark::runParallel(
    const std::vector<workbench::thermal::ThermalParameters>& cases, size_t numWorkers) {
    
    if (numWorkers <= 1) {
        return runSerial(cases);
    }

    std::vector<workbench::thermal::ThermalSolveResult> results(cases.size());
    ThreadPool pool(numWorkers);
    runParallel(cases, pool, results);
    return results;
}

double PerformanceBenchmark::compareResults(
    const std::vector<workbench::thermal::ThermalSolveResult>& resA,
    const std::vector<workbench::thermal::ThermalSolveResult>& resB) {
    
    if (resA.size() != resB.size()) {
        return 1.0e9; // Size mismatch indicates error
    }

    double maxDiff = 0.0;
    for (size_t i = 0; i < resA.size(); ++i) {
        const auto& nodesA = resA[i].nodes;
        const auto& nodesB = resB[i].nodes;
        if (nodesA.size() != nodesB.size()) {
            return 1.0e9;
        }
        for (size_t j = 0; j < nodesA.size(); ++j) {
            double diff = std::abs(nodesA[j].tFem - nodesB[j].tFem);
            if (diff > maxDiff) {
                maxDiff = diff;
            }
        }
    }
    return maxDiff;
}

BenchmarkSuiteResult PerformanceBenchmark::runSuite(
    const BenchmarkConfig& config, double launchToReadyMs) {
    
    BenchmarkSuiteResult suite;
    suite.config = config;
    suite.launchToReadyMs = launchToReadyMs;

    std::string configErr;
    if (!config.validate(configErr)) {
        suite.success = false;
        suite.errorMessage = configErr;
        return suite;
    }

    // Determine current ISO timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuffer;
#if defined(_WIN32)
    gmtime_s(&tmBuffer, &nowTime);
#else
    gmtime_r(&nowTime, &tmBuffer);
#endif
    char timeStr[64];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", &tmBuffer);
    suite.timestamp = timeStr;

    // Cap worker counts at available logical hardware concurrency
    const size_t maxConcurrency = std::thread::hardware_concurrency();
    std::vector<size_t> sanitizedWorkers;
    for (auto w : config.workerCounts) {
        size_t clamped = std::min(w, maxConcurrency);
        if (std::find(sanitizedWorkers.begin(), sanitizedWorkers.end(), clamped) == sanitizedWorkers.end()) {
            sanitizedWorkers.push_back(clamped);
        }
    }
    std::sort(sanitizedWorkers.begin(), sanitizedWorkers.end());

    // Generate reproducible workload batch
    const auto cases = generateWorkload(config.numCases, config.numElements, config.randomSeed);

    // Warm-up serial execution
    auto serialBaseline = runSerial(cases);

    // Serial baseline benchmark to establish reference
    std::vector<double> serialRepetitions;
    serialRepetitions.reserve(config.repetitions);
    for (size_t r = 0; r < config.repetitions; ++r) {
        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = runSerial(cases);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        serialRepetitions.push_back(ms);
    }
    auto sortedSerial = serialRepetitions;
    std::sort(sortedSerial.begin(), sortedSerial.end());
    double serialMedianMs = sortedSerial[sortedSerial.size() / 2];
    suite.serialBaselineMedianMs = serialMedianMs;

    suite.allVerified = true;
    double t1ReferenceMedianMs = serialMedianMs;

    // Benchmark each worker configuration using pre-created thread pool (measures compute only)
    for (size_t w : sanitizedWorkers) {
        WorkerRunResult wRes;
        wRes.workerCount = w;

        // Initialize bounded worker pool once per worker count to eliminate thread creation latency from timing
        ThreadPool pool(w);
        std::vector<workbench::thermal::ThermalSolveResult> lastRunResults(cases.size());

        // Dedicated warm-up run to prime CPU caches, branch predictor, and thread worker loops
        runParallel(cases, pool, lastRunResults);

        // Execute measured repetitions
        wRes.repetitionTimesMs.reserve(config.repetitions);
        for (size_t r = 0; r < config.repetitions; ++r) {
            auto t0 = std::chrono::high_resolution_clock::now();
            runParallel(cases, pool, lastRunResults);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            wRes.repetitionTimesMs.push_back(ms);
        }

        // Statistical calculations
        auto sortedTimes = wRes.repetitionTimesMs;
        std::sort(sortedTimes.begin(), sortedTimes.end());
        wRes.medianTimeMs = sortedTimes[sortedTimes.size() / 2];
        wRes.minTimeMs = sortedTimes.front();
        wRes.maxTimeMs = sortedTimes.back();

        double sum = std::accumulate(wRes.repetitionTimesMs.begin(), wRes.repetitionTimesMs.end(), 0.0);
        wRes.meanTimeMs = sum / static_cast<double>(wRes.repetitionTimesMs.size());

        if (wRes.repetitionTimesMs.size() > 1) {
            double sqSum = 0.0;
            for (double tVal : wRes.repetitionTimesMs) {
                sqSum += (tVal - wRes.meanTimeMs) * (tVal - wRes.meanTimeMs);
            }
            wRes.stdDevTimeMs = std::sqrt(sqSum / static_cast<double>(wRes.repetitionTimesMs.size() - 1));
        } else {
            wRes.stdDevTimeMs = 0.0;
        }

        if (w == 1) {
            t1ReferenceMedianMs = wRes.medianTimeMs;
        }

        // Numerical verification against serial baseline (must be <= 1.0e-8 °C)
        wRes.maxTempDiffVsSerial = compareResults(serialBaseline, lastRunResults);
        wRes.verified = (wRes.maxTempDiffVsSerial <= 1.0e-8);

        if (!wRes.verified) {
            suite.allVerified = false;
        }

        suite.workerResults.push_back(wRes);
    }

    // Compute Speedup and Efficiency relative to T1
    for (auto& wRes : suite.workerResults) {
        wRes.speedup = (wRes.medianTimeMs > 0.0) ? (t1ReferenceMedianMs / wRes.medianTimeMs) : 1.0;
        wRes.efficiency = wRes.speedup / static_cast<double>(wRes.workerCount);
    }

    suite.success = true;
    return suite;
}

} // namespace workbench::performance
