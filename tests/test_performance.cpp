#include "performance/ThreadPool.hpp"
#include "performance/PerformanceBenchmark.hpp"
#include "thermal/ThermalSolver1D.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <atomic>
#include <chrono>

using namespace workbench::performance;
using namespace workbench::thermal;

#define BENCH_REQUIRE(cond, msg) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + (msg)); \
        } \
    } while(0)

void testThreadPoolBasic() {
    std::cout << "[TEST 1] Testing ThreadPool basic execution & concurrency..." << std::endl;

    const size_t numThreads = 4;
    ThreadPool pool(numThreads);
    BENCH_REQUIRE(pool.getThreadCount() == numThreads, "Thread count mismatch");

    std::atomic<int> counter{0};
    const int totalTasks = 100;

    for (int i = 0; i < totalTasks; ++i) {
        pool.enqueue([&counter]() {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.waitAll();
    BENCH_REQUIRE(counter.load() == totalTasks, "Not all tasks completed");
    std::cout << "  -> Passed: All 100 tasks executed without loss or deadlock." << std::endl;
}

void testReproducibleWorkload() {
    std::cout << "[TEST 2] Testing deterministic PRNG workload generation..." << std::endl;

    const size_t numCases = 50;
    const uint32_t numElements = 20;
    const uint32_t seed = 42;

    auto batch1 = PerformanceBenchmark::generateWorkload(numCases, numElements, seed);
    auto batch2 = PerformanceBenchmark::generateWorkload(numCases, numElements, seed);

    BENCH_REQUIRE(batch1.size() == numCases, "Batch 1 size mismatch");
    BENCH_REQUIRE(batch2.size() == numCases, "Batch 2 size mismatch");

    for (size_t i = 0; i < numCases; ++i) {
        BENCH_REQUIRE(batch1[i].conductivity == batch2[i].conductivity, "Conductivity mismatch");
        BENCH_REQUIRE(batch1[i].heatGeneration == batch2[i].heatGeneration, "Heat generation mismatch");
        BENCH_REQUIRE(batch1[i].tLeft == batch2[i].tLeft, "tLeft mismatch");
        BENCH_REQUIRE(batch1[i].tRight == batch2[i].tRight, "tRight mismatch");
        BENCH_REQUIRE(batch1[i].length == batch2[i].length, "Length mismatch");
        BENCH_REQUIRE(batch1[i].area == batch2[i].area, "Area mismatch");
        BENCH_REQUIRE(batch1[i].numElements == numElements, "Element count mismatch");

        // Verify physical positiveness
        BENCH_REQUIRE(batch1[i].conductivity > 0.0, "Conductivity must be positive");
        BENCH_REQUIRE(batch1[i].heatGeneration > 0.0, "Heat generation must be positive");
        BENCH_REQUIRE(batch1[i].length > 0.0, "Length must be positive");
        BENCH_REQUIRE(batch1[i].area > 0.0, "Area must be positive");
    }
    std::cout << "  -> Passed: Fixed seed 42 produces identical, physically valid cases." << std::endl;
}

void testSerialVsParallelNumericalEquivalence() {
    std::cout << "[TEST 3] Testing Serial vs Parallel numerical equivalence (Tol <= 1e-8 °C)..." << std::endl;

    const size_t numCases = 100;
    const uint32_t numElements = 50;
    auto cases = PerformanceBenchmark::generateWorkload(numCases, numElements, 42);

    auto serialResults = PerformanceBenchmark::runSerial(cases);
    BENCH_REQUIRE(serialResults.size() == numCases, "Serial results size mismatch");

    for (size_t workers = 1; workers <= 4; ++workers) {
        auto parallelResults = PerformanceBenchmark::runParallel(cases, workers);
        BENCH_REQUIRE(parallelResults.size() == numCases, "Parallel results size mismatch");

        double maxDiff = PerformanceBenchmark::compareResults(serialResults, parallelResults);
        std::cout << "  -> Workers: " << workers << " | Max Temperature Discrepancy: " 
                  << maxDiff << " °C" << std::endl;

        BENCH_REQUIRE(maxDiff <= 1.0e-8, "Temperature discrepancy exceeds 1e-8 °C tolerance");
        BENCH_REQUIRE(maxDiff >= 0.0, "Discrepancy cannot be negative");
    }
    std::cout << "  -> Passed: Max error strictly <= 1e-8 °C across 1, 2, 3, and 4 workers." << std::endl;
}

void testDeterministicResultOrdering() {
    std::cout << "[TEST 4] Testing deterministic result slot ordering..." << std::endl;

    const size_t numCases = 60;
    const uint32_t numElements = 30;
    auto cases = PerformanceBenchmark::generateWorkload(numCases, numElements, 99);

    auto serialResults = PerformanceBenchmark::runSerial(cases);
    auto parallelResults = PerformanceBenchmark::runParallel(cases, 4);

    for (size_t i = 0; i < numCases; ++i) {
        BENCH_REQUIRE(parallelResults[i].nodes.size() == serialResults[i].nodes.size(), "Node count mismatch");
        for (size_t j = 0; j < parallelResults[i].nodes.size(); ++j) {
            double diff = std::abs(parallelResults[i].nodes[j].tFem - serialResults[i].nodes[j].tFem);
            BENCH_REQUIRE(diff <= 1.0e-8, "Per-node deterministic slot mismatch");
        }
    }
    std::cout << "  -> Passed: Result indices strictly preserve case ordering." << std::endl;
}

void testBenchmarkSuiteExecution() {
    std::cout << "[TEST 5] Testing full BenchmarkSuiteResult (5 repetitions & CSV output)..." << std::endl;

    BenchmarkConfig config;
    config.numCases = 200;
    config.numElements = 80;
    config.randomSeed = 42;
    config.workerCounts = {1, 2, 3, 4};
    config.repetitions = 5;

    auto suite = PerformanceBenchmark::runSuite(config, 15.2);
    BENCH_REQUIRE(suite.success, "Benchmark suite execution failed");
    BENCH_REQUIRE(suite.allVerified, "Verification failed for one or more worker configurations");
    BENCH_REQUIRE(!suite.workerResults.empty(), "Worker results empty");

    for (const auto& w : suite.workerResults) {
        BENCH_REQUIRE(w.repetitionTimesMs.size() == 5, "Repetition count must be exactly 5");
        BENCH_REQUIRE(w.medianTimeMs > 0.0, "Median time must be positive");
        BENCH_REQUIRE(w.speedup > 0.0, "Speedup must be positive");
        BENCH_REQUIRE(w.efficiency > 0.0, "Efficiency must be positive");
        BENCH_REQUIRE(w.verified, "Worker must be verified");
        BENCH_REQUIRE(w.maxTempDiffVsSerial <= 1.0e-8, "Worker discrepancy exceeds tolerance");
        std::cout << "  -> Worker Count: " << w.workerCount 
                  << " | Median: " << w.medianTimeMs << " ms"
                  << " | Speedup: " << w.speedup 
                  << " | Efficiency: " << w.efficiency 
                  << " | Verified: " << (w.verified ? "PASS" : "FAIL") << std::endl;
    }

    std::string csv = suite.toCsv();
    BENCH_REQUIRE(!csv.empty(), "CSV output empty");
    BENCH_REQUIRE(csv.find("workers,run1_ms,run2_ms") != std::string::npos, "CSV header missing");
    BENCH_REQUIRE(csv.find("PASS") != std::string::npos, "CSV verification status missing");
    BENCH_REQUIRE(csv.find("Hardware Audit Metadata:") != std::string::npos, "CSV must contain audit metadata");
    BENCH_REQUIRE(csv.find("CPU Processor:") != std::string::npos, "CSV must contain CPU model");
    BENCH_REQUIRE(csv.find("Logical Cores:") != std::string::npos, "CSV must contain logical cores");
    BENCH_REQUIRE(csv.find("Installed RAM:") != std::string::npos, "CSV must contain RAM");
    BENCH_REQUIRE(csv.find("Compiler Optimization Flags:") != std::string::npos, "CSV must contain compiler flags");
    std::cout << "  -> Passed: Full benchmark suite executed and auditable CSV validated." << std::endl;
}

void testSingleCpuGracefulHandling() {
    std::cout << "[TEST 6] Testing single-CPU and capped worker graceful handling..." << std::endl;

    BenchmarkConfig config;
    config.numCases = 20;
    config.numElements = 15;
    config.randomSeed = 101;
    config.workerCounts = {1, 2, 8}; // Requesting 8 workers should be clamped to logical cores
    config.repetitions = 5;

    auto suite = PerformanceBenchmark::runSuite(config, 10.0);
    BENCH_REQUIRE(suite.success, "Benchmark suite must succeed without crashing");
    BENCH_REQUIRE(suite.allVerified, "All results must be verified <= 1e-8 C");
    BENCH_REQUIRE(!suite.workerResults.empty(), "Must return worker results");

    for (const auto& w : suite.workerResults) {
        BENCH_REQUIRE(w.workerCount <= 4, "Worker count must be capped at min(4, logical CPUs)");
        BENCH_REQUIRE(!w.assessmentNote.empty(), "Assessment note must be populated");
        BENCH_REQUIRE(w.verified, "Result must satisfy numerical tolerance");
    }
    std::cout << "  -> Passed: Single-CPU and capped worker queries handled gracefully." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  UNIFIED ENGINEERING WORKBENCH - PERFORMANCE LAB " << std::endl;
    std::cout << "      MULTICORE SCALABILITY & VERIFICATION TESTS   " << std::endl;
    std::cout << "==================================================" << std::endl;

    try {
        testThreadPoolBasic();
        testReproducibleWorkload();
        testSerialVsParallelNumericalEquivalence();
        testDeterministicResultOrdering();
        testBenchmarkSuiteExecution();
        testSingleCpuGracefulHandling();

        std::cout << "\n>>> ALL 6/6 PERFORMANCE BENCHMARK TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL TEST EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
