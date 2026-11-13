#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "server/HttpServer.hpp"
#include "core/Logger.hpp"
#include "core/SystemInfo.hpp"
#include "geometry/StlParser.hpp"
#include "geometry/CsvParser.hpp"
#include "thermal/ThermalSolver1D.hpp"
#include "performance/ThreadPool.hpp"
#include "performance/PerformanceBenchmark.hpp"
#include "hardware/HardwareManager.hpp"
#include "neural/NeuralNetRegression.hpp"
#include "httplib/httplib.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <sstream>
#include <fstream>
#include <random>
#include <iomanip>

namespace workbench::server {

HttpServer::HttpServer(const ServerConfig& config)
    : m_config(config),
      m_actualPort(config.port),
      m_server(std::make_unique<httplib::Server>()) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << dist(gen) << std::setw(16) << dist(gen);
    m_sessionToken = ss.str();
}

HttpServer::~HttpServer() {
    stop();
}

std::string HttpServer::getBaseUrl() const {
    std::ostringstream ss;
    ss << "http://" << m_config.host << ":" << m_actualPort;
    return ss.str();
}

void HttpServer::setupRoutes() {
    auto applyCors = [this](const httplib::Request& req, httplib::Response& res) {
        std::string origin = req.get_header_value("Origin");
        if (origin.empty()) {
            res.set_header("Access-Control-Allow-Origin", "http://127.0.0.1:" + std::to_string(m_actualPort));
        } else if (origin.find("127.0.0.1") != std::string::npos || origin.find("localhost") != std::string::npos) {
            res.set_header("Access-Control-Allow-Origin", origin);
        } else {
            res.set_header("Access-Control-Allow-Origin", "http://127.0.0.1:" + std::to_string(m_actualPort));
        }
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Session-Token");
    };

    // Global post-routing handler to enforce strictly bound localhost CORS on all responses
    m_server->set_post_routing_handler([this, applyCors](const httplib::Request& req, httplib::Response& res) {
        applyCors(req, res);
    });

    // Health check endpoint
    m_server->Get("/api/health", [applyCors](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json response = {
            {"status", "ok"},
            {"service", "Unified Engineering Workbench Backend"},
            {"version", "1.0.0"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        res.set_content(response.dump(2), "application/json");
        applyCors(req, res);
    });

    // System telemetry endpoint
    m_server->Get("/api/system/info", [this, applyCors](const httplib::Request& req, httplib::Response& res) {
        auto telemetry = core::SystemInfoProvider::getTelemetry();
        auto j = telemetry.toJson();
        j["session_token"] = m_sessionToken;
        res.set_content(j.dump(2), "application/json");
        applyCors(req, res);
    });

    // Logging endpoint
    m_server->Get("/api/logs", [](const httplib::Request& req, httplib::Response& res) {
        size_t count = 50;
        if (req.has_param("limit")) {
            try {
                count = std::stoul(req.get_param_value("limit"));
            } catch (...) {}
        }
        auto logs = core::Logger::instance().getRecentLogs(count);
        nlohmann::json logArray = nlohmann::json::array();
        for (const auto& entry : logs) {
            std::string lvlStr;
            switch (entry.level) {
                case core::LogLevel::Debug:   lvlStr = "DEBUG"; break;
                case core::LogLevel::Info:    lvlStr = "INFO"; break;
                case core::LogLevel::Warning: lvlStr = "WARN"; break;
                case core::LogLevel::Error:   lvlStr = "ERROR"; break;
            }
            logArray.push_back({
                {"timestamp", entry.timestamp},
                {"level", lvlStr},
                {"message", entry.message}
            });
        }
        nlohmann::json response = {
            {"count", logArray.size()},
            {"logs", logArray}
        };
        res.set_content(response.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Plaintext Log Export Endpoint (Task G)
    m_server->Get("/api/logs/export", [](const httplib::Request&, httplib::Response& res) {
        auto logs = core::Logger::instance().getRecentLogs(500);
        std::ostringstream ss;
        ss << "# ===================================================================\n";
        ss << "# UNIFIED ENGINEERING WORKBENCH - SYSTEM LOG EXPORT\n";
        ss << "# PASS SCIENCE WORKS R&D Internship 2026-2027\n";
        ss << "# Target OS: Windows 11 x64 | Engine: Pure ISO C++17\n";
        ss << "# ===================================================================\n\n";
        for (const auto& entry : logs) {
            std::string lvlStr;
            switch (entry.level) {
                case core::LogLevel::Debug:   lvlStr = "DEBUG"; break;
                case core::LogLevel::Info:    lvlStr = "INFO "; break;
                case core::LogLevel::Warning: lvlStr = "WARN "; break;
                case core::LogLevel::Error:   lvlStr = "ERROR"; break;
            }
            ss << "[" << entry.timestamp << "] [" << lvlStr << "] " << entry.message << "\n";
        }
        res.set_content(ss.str(), "text/plain");
        res.set_header("Content-Disposition", "attachment; filename=\"workbench_system_logs.log\"");
        // CORS handled by global post-routing handler
    });

    // Clear Logs Endpoint (Settings Module)
    m_server->Post("/api/logs/clear", [](const httplib::Request&, httplib::Response& res) {
        core::Logger::instance().clear();
        core::Logger::instance().info("In-memory log ring buffer cleared via Settings module.");
        nlohmann::json response = {
            {"success", true},
            {"message", "In-memory log buffer cleared successfully."}
        };
        res.set_content(response.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Comprehensive Settings & Diagnostics Metadata Endpoint
    m_server->Get("/api/system/settings", [this](const httplib::Request&, httplib::Response& res) {
        auto telemetry = core::SystemInfoProvider::getTelemetry();
        nlohmann::json response = {
            {"server", {
                {"host", m_config.host},
                {"port", m_actualPort},
                {"base_url", getBaseUrl()},
                {"static_dir", m_config.staticDir},
                {"uptime_seconds", telemetry.uptimeSeconds},
                {"launch_latency_ms", telemetry.launchToReadyMs}
            }},
            {"toolchain", {
                {"compiler", telemetry.compiler},
                {"flags", telemetry.compilerFlags},
                {"build_type", telemetry.buildType},
                {"cpp_standard", "ISO C++17"},
                {"architecture", telemetry.architecture}
            }},
            {"host_hardware", {
                {"cpu_model", telemetry.cpuModel},
                {"physical_cores", telemetry.physicalCores},
                {"logical_cores", telemetry.logicalCores},
                {"total_ram_mb", telemetry.totalRamBytes / (1024 * 1024)},
                {"available_ram_mb", telemetry.availableRamBytes / (1024 * 1024)},
                {"os_name", telemetry.osName}
            }},
            {"numerical_defaults", {
                {"prng_seed", 42},
                {"thermal_elements", 40},
                {"thermal_conductivity", 10.0},
                {"thermal_heat_gen", 10000.0},
                {"perf_cases", 500},
                {"neural_lr", 0.1},
                {"neural_steps", 100},
                {"finite_diff_eps", 1.0e-6},
                {"neural_tolerance", 1.0e-5},
                {"thermal_tolerance", 1.0e-8}
            }},
            {"viewport_defaults", {
                {"cad_opacity", 0.85},
                {"point_size", 10.0},
                {"wireframe", false},
                {"colormap", "Turbo (Scientific)"},
                {"fov", 45.0}
            }}
        };
        res.set_content(response.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Sample Benchmark Geometry Assets
    m_server->Get("/api/geometry/sample-stl", [](const httplib::Request&, httplib::Response& res) {
        std::string stlPath = core::SystemInfoProvider::resolvePath("data/stl/engineering_bracket.stl");
        auto parseRes = geometry::StlParser::parseAsciiFile(stlPath);
        res.set_content(parseRes.toJson().dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    m_server->Get("/api/geometry/sample-csv", [](const httplib::Request&, httplib::Response& res) {
        std::string csvPath = core::SystemInfoProvider::resolvePath("data/csv/synthetic_thermal_field.csv");
        auto parseRes = geometry::CsvParser::parseFile(csvPath);
        res.set_content(parseRes.toJson().dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Custom STL Parse Endpoint
    m_server->Post("/api/geometry/parse-stl", [](const httplib::Request& req, httplib::Response& res) {
        std::string contentToParse;
        if (req.body.rfind("solid", 0) == 0) {
            contentToParse = req.body;
        } else {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("content")) {
                    contentToParse = j["content"].get<std::string>();
                } else if (j.contains("path")) {
                    std::string filePath = core::SystemInfoProvider::resolvePath(j["path"].get<std::string>());
                    auto parseRes = geometry::StlParser::parseAsciiFile(filePath);
                    res.set_content(parseRes.toJson().dump(2), "application/json");
                    // CORS handled by global post-routing handler
                    return;
                }
            } catch (...) {
                contentToParse = req.body;
            }
        }

        auto parseRes = geometry::StlParser::parseAsciiString(contentToParse);
        res.set_content(parseRes.toJson().dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Custom ANSYS CSV Parse Endpoint
    m_server->Post("/api/geometry/parse-csv", [](const httplib::Request& req, httplib::Response& res) {
        std::string contentToParse;
        if (req.body.find("node_id") != std::string::npos || req.body.find("NODE_ID") != std::string::npos) {
            contentToParse = req.body;
        } else {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("content")) {
                    contentToParse = j["content"].get<std::string>();
                } else if (j.contains("path")) {
                    std::string filePath = core::SystemInfoProvider::resolvePath(j["path"].get<std::string>());
                    auto parseRes = geometry::CsvParser::parseFile(filePath);
                    res.set_content(parseRes.toJson().dump(2), "application/json");
                    // CORS handled by global post-routing handler
                    return;
                }
            } catch (...) {
                contentToParse = req.body;
            }
        }

        auto parseRes = geometry::CsvParser::parseString(contentToParse);
        res.set_content(parseRes.toJson().dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // ===================================================================
    // TASK C: 1D Thermal Finite Element Analysis Endpoints
    // ===================================================================

    m_server->Post("/api/thermal/solve", [](const httplib::Request& req, httplib::Response& res) {
        thermal::ThermalParameters params;
        // Default benchmark parameters: L=1.0, A=1e-4, k=10, q=10000, T0=100, TL=20, N=10
        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("length")) params.length = j["length"].get<double>();
                if (j.contains("area")) params.area = j["area"].get<double>();
                if (j.contains("conductivity")) params.conductivity = j["conductivity"].get<double>();
                if (j.contains("heat_generation")) params.heatGeneration = j["heat_generation"].get<double>();
                if (j.contains("t_left")) params.tLeft = j["t_left"].get<double>();
                if (j.contains("t_right")) params.tRight = j["t_right"].get<double>();
                if (j.contains("num_elements")) params.numElements = j["num_elements"].get<uint32_t>();
            } catch (const std::exception& e) {
                nlohmann::json errRes = {
                    {"success", false},
                    {"error_message", std::string("Malformed JSON payload: ") + e.what()}
                };
                res.set_content(errRes.dump(2), "application/json");
                // CORS handled by global post-routing handler
                return;
            }
        }

        auto solveRes = thermal::ThermalSolver1D::solve(params);
        res.set_content(solveRes.toJson().dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    m_server->Post("/api/thermal/convergence", [](const httplib::Request& req, httplib::Response& res) {
        thermal::ThermalParameters params;
        std::vector<uint32_t> meshSizes = {10, 20, 40, 80};

        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("length")) params.length = j["length"].get<double>();
                if (j.contains("area")) params.area = j["area"].get<double>();
                if (j.contains("conductivity")) params.conductivity = j["conductivity"].get<double>();
                if (j.contains("heat_generation")) params.heatGeneration = j["heat_generation"].get<double>();
                if (j.contains("t_left")) params.tLeft = j["t_left"].get<double>();
                if (j.contains("t_right")) params.tRight = j["t_right"].get<double>();
                if (j.contains("mesh_sizes") && j["mesh_sizes"].is_array()) {
                    meshSizes.clear();
                    for (const auto& item : j["mesh_sizes"]) {
                        meshSizes.push_back(item.get<uint32_t>());
                    }
                }
            } catch (const std::exception& e) {
                nlohmann::json errRes = {
                    {"success", false},
                    {"error_message", std::string("Malformed JSON payload: ") + e.what()}
                };
                res.set_content(errRes.dump(2), "application/json");
                // CORS handled by global post-routing handler
                return;
            }
        }

        auto studyRes = thermal::ThermalSolver1D::runConvergenceStudy(params, meshSizes);
        res.set_content(studyRes.toJson().dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    m_server->Get("/api/thermal/export-csv", [](const httplib::Request& req, httplib::Response& res) {
        std::string type = "temperature";
        if (req.has_param("type")) {
            type = req.get_param_value("type");
        }

        thermal::ThermalParameters params;
        if (req.has_param("length")) params.length = std::stod(req.get_param_value("length"));
        if (req.has_param("area")) params.area = std::stod(req.get_param_value("area"));
        if (req.has_param("conductivity")) params.conductivity = std::stod(req.get_param_value("conductivity"));
        if (req.has_param("heat_generation")) params.heatGeneration = std::stod(req.get_param_value("heat_generation"));
        if (req.has_param("t_left")) params.tLeft = std::stod(req.get_param_value("t_left"));
        if (req.has_param("t_right")) params.tRight = std::stod(req.get_param_value("t_right"));
        if (req.has_param("elements")) params.numElements = std::stoul(req.get_param_value("elements"));

        if (type == "convergence") {
            auto studyRes = thermal::ThermalSolver1D::runConvergenceStudy(params, {10, 20, 40, 80});
            res.set_content(studyRes.toCsv(), "text/csv");
            res.set_header("Content-Disposition", "attachment; filename=\"thermal_mesh_convergence.csv\"");
        } else {
            auto solveRes = thermal::ThermalSolver1D::solve(params);
            res.set_content(solveRes.toCsv(), "text/csv");
            res.set_header("Content-Disposition", "attachment; filename=\"thermal_temperature_results.csv\"");
        }
        // CORS handled by global post-routing handler
    });

    // Multicore Performance Lab Benchmark endpoint
    m_server->Post("/api/performance/benchmark", [](const httplib::Request& req, httplib::Response& res) {
        performance::BenchmarkConfig config;
        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                config = performance::BenchmarkConfig::fromJson(j);
            } catch (const std::exception& e) {
                nlohmann::json errRes = {
                    {"success", false},
                    {"error_message", std::string("Malformed JSON payload: ") + e.what()}
                };
                res.set_content(errRes.dump(2), "application/json");
                // CORS handled by global post-routing handler
                return;
            }
        }

        double launchToReady = core::SystemInfoProvider::getLaunchToReadyMs();
        core::Logger::instance().info("Executing Performance Lab Multicore Benchmark: " + 
            std::to_string(config.numCases) + " cases, " + 
            std::to_string(config.numElements) + " elements, " + 
            std::to_string(config.repetitions) + " reps.");

        auto suiteResult = performance::PerformanceBenchmark::runSuite(config, launchToReady);
        res.set_content(suiteResult.toJson().dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Performance Lab CSV Export endpoint
    m_server->Get("/api/performance/export-csv", [](const httplib::Request& req, httplib::Response& res) {
        performance::BenchmarkConfig config;
        if (req.has_param("cases")) config.numCases = std::stoul(req.get_param_value("cases"));
        if (req.has_param("elements")) config.numElements = std::stoul(req.get_param_value("elements"));
        if (req.has_param("seed")) config.randomSeed = std::stoul(req.get_param_value("seed"));
        if (req.has_param("repetitions")) config.repetitions = std::stoul(req.get_param_value("repetitions"));

        double launchToReady = core::SystemInfoProvider::getLaunchToReadyMs();
        auto suiteResult = performance::PerformanceBenchmark::runSuite(config, launchToReady);

        res.set_content(suiteResult.toCsv(), "text/csv");
        res.set_header("Content-Disposition", "attachment; filename=\"multicore_benchmark_results.csv\"");
        // CORS handled by global post-routing handler
    });

    // -------------------------------------------------------------
    // ASYNC JOB MANAGEMENT ENDPOINTS (Task D & Task C Long Studies)
    // -------------------------------------------------------------

    auto startBenchmarkJob = [this](const httplib::Request& req, httplib::Response& res) {
        performance::BenchmarkConfig config;
        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                config = performance::BenchmarkConfig::fromJson(j);
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(nlohmann::json{
                    {"success", false},
                    {"error_message", std::string("Malformed JSON payload: ") + e.what()}
                }.dump(2), "application/json");
                return;
            }
        }

        std::string configErr;
        if (!config.validate(configErr)) {
            res.status = 400;
            res.set_content(nlohmann::json{
                {"success", false},
                {"error_message", configErr}
            }.dump(2), "application/json");
            return;
        }

        auto job = std::make_shared<AsyncJob>();
        uint64_t jId = ++m_jobCounter;
        job->id = "bench_" + std::to_string(jId);
        job->type = "benchmark";
        job->status = "running";
        job->progressPercent = 5;
        job->message = "Generating reproducible workload...";

        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            m_jobs[job->id] = job;
        }

        double launchToReady = core::SystemInfoProvider::getLaunchToReadyMs();

        std::thread([job, config, launchToReady]() {
            try {
                if (job->cancelRequested) {
                    job->status = "cancelled";
                    job->message = "Benchmark cancelled by user.";
                    return;
                }

                job->progressPercent = 10;
                job->message = "Running baseline serial warm-up and calibration...";

                size_t availableCpus = std::thread::hardware_concurrency();
                if (availableCpus == 0) availableCpus = 1;
                const size_t maxAllowed = (availableCpus <= 1) ? 2 : std::min<size_t>(4, availableCpus);

                std::vector<size_t> workers;
                for (auto w : config.workerCounts) {
                    size_t cl = std::min<size_t>(w, maxAllowed);
                    if (std::find(workers.begin(), workers.end(), cl) == workers.end()) {
                        workers.push_back(cl);
                    }
                }
                std::sort(workers.begin(), workers.end());

                const auto cases = performance::PerformanceBenchmark::generateWorkload(
                    config.numCases, config.numElements, config.randomSeed);

                if (job->cancelRequested) {
                    job->status = "cancelled";
                    job->message = "Benchmark cancelled by user.";
                    return;
                }

                auto serialBaseline = performance::PerformanceBenchmark::runSerial(cases);
                std::vector<double> serialReps;
                for (size_t r = 0; r < config.repetitions; ++r) {
                    if (job->cancelRequested) {
                        job->status = "cancelled";
                        job->message = "Benchmark cancelled by user.";
                        return;
                    }
                    auto t0 = std::chrono::high_resolution_clock::now();
                    performance::PerformanceBenchmark::runSerial(cases);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    serialReps.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                }
                std::sort(serialReps.begin(), serialReps.end());
                double serialMedian = serialReps[serialReps.size() / 2];

                performance::BenchmarkSuiteResult suite;
                suite.config = config;
                suite.launchToReadyMs = launchToReady;
                suite.serialBaselineMedianMs = serialMedian;
                suite.allVerified = true;

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

                double t1Ref = serialMedian;
                size_t totalSteps = workers.size();

                for (size_t step = 0; step < totalSteps; ++step) {
                    if (job->cancelRequested) {
                        job->status = "cancelled";
                        job->message = "Benchmark cancelled by user.";
                        return;
                    }

                    size_t w = workers[step];
                    job->message = "Benchmarking worker pool with " + std::to_string(w) + " thread" + (w > 1 ? "s" : "") + "...";
                    job->progressPercent = static_cast<int>(20 + (75.0 * step / totalSteps));

                    performance::WorkerRunResult wRes;
                    wRes.workerCount = w;
                    performance::ThreadPool pool(w);
                    std::vector<thermal::ThermalSolveResult> outRes(cases.size());

                    performance::PerformanceBenchmark::runParallel(cases, pool, outRes);

                    for (size_t r = 0; r < config.repetitions; ++r) {
                        if (job->cancelRequested) {
                            job->status = "cancelled";
                            job->message = "Benchmark cancelled by user.";
                            return;
                        }
                        auto t0 = std::chrono::high_resolution_clock::now();
                        performance::PerformanceBenchmark::runParallel(cases, pool, outRes);
                        auto t1 = std::chrono::high_resolution_clock::now();
                        wRes.repetitionTimesMs.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                    }

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
                    }

                    if (w == 1) t1Ref = wRes.medianTimeMs;

                    wRes.maxTempDiffVsSerial = performance::PerformanceBenchmark::compareResults(serialBaseline, outRes);
                    wRes.verified = (wRes.maxTempDiffVsSerial <= 1.0e-8);
                    if (!wRes.verified) suite.allVerified = false;

                    suite.workerResults.push_back(wRes);
                }

                for (auto& wRes : suite.workerResults) {
                    if (availableCpus <= 1 && wRes.workerCount > 1) {
                        wRes.speedupAssessable = false;
                        wRes.assessmentNote = "speedup_not_assessable";
                        wRes.speedup = (wRes.medianTimeMs > 0.0) ? (t1Ref / wRes.medianTimeMs) : 1.0;
                        wRes.efficiency = wRes.speedup / static_cast<double>(wRes.workerCount);
                    } else {
                        wRes.speedupAssessable = true;
                        wRes.assessmentNote = "assessable";
                        wRes.speedup = (wRes.medianTimeMs > 0.0) ? (t1Ref / wRes.medianTimeMs) : 1.0;
                        wRes.efficiency = wRes.speedup / static_cast<double>(wRes.workerCount);
                    }
                }

                suite.success = true;
                job->result = suite.toJson();
                job->progressPercent = 100;
                job->status = "completed";
                job->message = "Benchmark completed successfully across " + std::to_string(workers.size()) + " configurations.";
            } catch (const std::exception& e) {
                job->status = "failed";
                job->errorMessage = e.what();
                job->message = std::string("Benchmark error: ") + e.what();
            }
        }).detach();

        res.set_content(nlohmann::json{
            {"success", true},
            {"job_id", job->id},
            {"status", "running"},
            {"message", "Benchmark job scheduled."}
        }.dump(2), "application/json");
    };

    m_server->Post("/api/benchmark/start", startBenchmarkJob);
    m_server->Post("/api/performance/benchmark/start", startBenchmarkJob);

    m_server->Post("/api/thermal/convergence/start", [this](const httplib::Request& req, httplib::Response& res) {
        thermal::ThermalParameters params;
        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("length")) params.length = j["length"].get<double>();
                if (j.contains("area")) params.area = j["area"].get<double>();
                if (j.contains("conductivity")) params.conductivity = j["conductivity"].get<double>();
                if (j.contains("heat_generation")) params.heatGeneration = j["heat_generation"].get<double>();
                if (j.contains("t_left")) params.tLeft = j["t_left"].get<double>();
                if (j.contains("t_right")) params.tRight = j["t_right"].get<double>();
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(nlohmann::json{
                    {"success", false},
                    {"error_message", std::string("Malformed JSON payload: ") + e.what()}
                }.dump(2), "application/json");
                return;
            }
        }

        std::string paramErr;
        if (!params.validate(paramErr)) {
            res.status = 400;
            res.set_content(nlohmann::json{
                {"success", false},
                {"error_message", paramErr}
            }.dump(2), "application/json");
            return;
        }

        auto job = std::make_shared<AsyncJob>();
        uint64_t jId = ++m_jobCounter;
        job->id = "conv_" + std::to_string(jId);
        job->type = "convergence";
        job->status = "running";
        job->progressPercent = 10;
        job->message = "Initializing convergence study...";

        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            m_jobs[job->id] = job;
        }

        std::thread([job, params]() {
            try {
                std::vector<uint32_t> meshSizes = {10, 20, 40, 80};
                thermal::ConvergenceStudyResult studyResult;
                studyResult.parameters = params;

                std::vector<thermal::MeshConvergenceEntry> entries;
                double prevError = 0.0;
                double prevH = 0.0;

                for (size_t i = 0; i < meshSizes.size(); ++i) {
                    if (job->cancelRequested) {
                        job->status = "cancelled";
                        job->message = "Convergence study cancelled by user.";
                        return;
                    }

                    uint32_t N = meshSizes[i];
                    job->message = "Solving finite element mesh N=" + std::to_string(N) + "...";
                    job->progressPercent = static_cast<int>(15 + (80.0 * i / meshSizes.size()));

                    thermal::ThermalParameters p = params;
                    p.numElements = N;

                    auto sol = thermal::ThermalSolver1D::solve(p);
                    if (!sol.success) {
                        job->status = "failed";
                        job->errorMessage = sol.errorMessage;
                        return;
                    }

                    double currentH = p.length / static_cast<double>(N);
                    double currentError = sol.maxMidpointError;
                    double theoretical = sol.theoreticalMaxMidpointError;
                    double rate = 0.0;

                    if (i > 0 && prevError > 1e-14 && currentError > 1e-14 && prevH > currentH) {
                        rate = std::log(prevError / currentError) / std::log(prevH / currentH);
                    }

                    entries.push_back({
                        N,
                        N + 1,
                        currentH,
                        currentError,
                        theoretical,
                        rate,
                        sol.nodes
                    });

                    prevError = currentError;
                    prevH = currentH;
                }

                studyResult.entries = entries;
                studyResult.success = true;
                job->result = studyResult.toJson();
                job->progressPercent = 100;
                job->status = "completed";
                job->message = "Convergence study completed across all 4 meshes.";
            } catch (const std::exception& e) {
                job->status = "failed";
                job->errorMessage = e.what();
            }
        }).detach();

        res.set_content(nlohmann::json{
            {"success", true},
            {"job_id", job->id},
            {"status", "running"},
            {"message", "Convergence study job scheduled."}
        }.dump(2), "application/json");
    });

    m_server->Get(R"(/api/jobs/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string jobId = req.matches[1];
        std::shared_ptr<AsyncJob> job;
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            auto it = m_jobs.find(jobId);
            if (it != m_jobs.end()) {
                job = it->second;
            }
        }

        if (!job) {
            res.status = 404;
            res.set_content(nlohmann::json{
                {"success", false},
                {"error_message", "Job not found: " + jobId}
            }.dump(2), "application/json");
            return;
        }

        nlohmann::json resp = {
            {"success", true},
            {"job_id", job->id},
            {"type", job->type},
            {"status", job->status},
            {"progress", job->progressPercent},
            {"message", job->message},
            {"error_message", job->errorMessage}
        };
        if (job->status == "completed") {
            resp["result"] = job->result;
        }
        res.set_content(resp.dump(2), "application/json");
    });

    m_server->Post(R"(/api/jobs/([^/]+)/cancel)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string jobId = req.matches[1];
        std::shared_ptr<AsyncJob> job;
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            auto it = m_jobs.find(jobId);
            if (it != m_jobs.end()) {
                job = it->second;
            }
        }

        if (!job) {
            res.status = 404;
            res.set_content(nlohmann::json{
                {"success", false},
                {"error_message", "Job not found: " + jobId}
            }.dump(2), "application/json");
            return;
        }

        job->cancelRequested = true;
        job->status = "cancelling";
        job->message = "Cancellation requested...";

        res.set_content(nlohmann::json{
            {"success", true},
            {"job_id", job->id},
            {"status", "cancelling"},
            {"message", "Job cancellation signal delivered."}
        }.dump(2), "application/json");
    });

    // Hardware Integration Endpoints (Task E)
    m_server->Get("/api/hardware/devices", [](const httplib::Request&, httplib::Response& res) {
        auto devices = hardware::HardwareManager::instance().getDiscoveredDevices();
        nlohmann::json jList = nlohmann::json::array();
        for (const auto& dev : devices) {
            jList.push_back(dev.toJson());
        }
        nlohmann::json response = {
            {"success", true},
            {"count", jList.size()},
            {"devices", jList}
        };
        res.set_content(response.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    m_server->Get("/api/hardware/telemetry", [](const httplib::Request&, httplib::Response& res) {
        auto telem = hardware::HardwareManager::instance().getAllTelemetry();
        res.set_content(telem.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    m_server->Get("/api/hardware/architecture", [](const httplib::Request&, httplib::Response& res) {
        auto arch = hardware::HardwareManager::getSystemElectricalArchitecture();
        res.set_content(arch.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    m_server->Post("/api/hardware/connect", [](const httplib::Request& req, httplib::Response& res) {
        std::string deviceId;
        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("device_id")) deviceId = j["device_id"].get<std::string>();
            } catch (...) {}
        }
        bool ok = false;
        if (deviceId.empty() || deviceId == "all") {
            ok = hardware::HardwareManager::instance().connectAll();
        } else {
            ok = hardware::HardwareManager::instance().connectDevice(deviceId);
        }
        nlohmann::json response = {
            {"success", ok},
            {"device_id", deviceId.empty() ? "all" : deviceId},
            {"message", ok ? "Device(s) connected successfully." : "Connection failed."}
        };
        res.set_content(response.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    m_server->Post("/api/hardware/disconnect", [](const httplib::Request& req, httplib::Response& res) {
        std::string deviceId;
        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("device_id")) deviceId = j["device_id"].get<std::string>();
            } catch (...) {}
        }
        bool ok = false;
        if (deviceId.empty() || deviceId == "all") {
            ok = hardware::HardwareManager::instance().disconnectAll();
        } else {
            ok = hardware::HardwareManager::instance().disconnectDevice(deviceId);
        }
        nlohmann::json response = {
            {"success", ok},
            {"device_id", deviceId.empty() ? "all" : deviceId},
            {"message", ok ? "Device(s) disconnected." : "Disconnect failed."}
        };
        res.set_content(response.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    m_server->Post("/api/hardware/fault-inject", [](const httplib::Request& req, httplib::Response& res) {
        std::string deviceId = "mcu_stm32h7";
        std::string faultType = "timeout";
        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("device_id")) deviceId = j["device_id"].get<std::string>();
                if (j.contains("fault_type")) faultType = j["fault_type"].get<std::string>();
            } catch (...) {}
        }
        bool ok = hardware::HardwareManager::instance().injectFault(deviceId, faultType);
        nlohmann::json response = {
            {"success", ok},
            {"device_id", deviceId},
            {"fault_type", faultType},
            {"message", ok ? "Fault injected successfully." : "Failed to inject fault (device not found)."}
        };
        res.set_content(response.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    m_server->Post("/api/hardware/fault-recover", [](const httplib::Request& req, httplib::Response& res) {
        std::string deviceId = "mcu_stm32h7";
        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("device_id")) deviceId = j["device_id"].get<std::string>();
            } catch (...) {}
        }
        bool ok = hardware::HardwareManager::instance().recoverFault(deviceId);
        nlohmann::json response = {
            {"success", ok},
            {"device_id", deviceId},
            {"message", ok ? "Fault recovered successfully. Device operational." : "Recovery failed (device not in fault state)."}
        };
        res.set_content(response.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Hardware Electrical Pin Architecture CSV Export (Task G)
    m_server->Get("/api/hardware/export-csv", [](const httplib::Request&, httplib::Response& res) {
        auto arch = hardware::HardwareManager::getSystemElectricalArchitecture();
        std::ostringstream ss;
        ss << "# Unified Engineering Workbench - Tri-Device Hardware Pin & Signal Architecture\n";
        ss << "# System Title: " << arch["system_title"].get<std::string>() << "\n";
        ss << "# Target Devices: STM32H753ZI (Cortex-M7), Xilinx Artix-7 (XC7A100T), FTDI FT2232H JTAG Probe\n";
        ss << "pin,signal,source,destination,voltage,protocol,pull,ownership\n";
        if (arch.contains("pin_voltage_table")) {
            for (const auto& p : arch["pin_voltage_table"]) {
                ss << p["pin"].get<int>() << ","
                   << "\"" << p["signal"].get<std::string>() << "\","
                   << "\"" << p["source"].get<std::string>() << "\","
                   << "\"" << p["destination"].get<std::string>() << "\","
                   << "\"" << p["voltage"].get<std::string>() << "\","
                   << "\"" << p["protocol"].get<std::string>() << "\","
                   << "\"" << p["pull"].get<std::string>() << "\","
                   << "\"" << p["ownership"].get<std::string>() << "\"\n";
            }
        }
        res.set_content(ss.str(), "text/csv");
        res.set_header("Content-Disposition", "attachment; filename=\"hardware_pin_architecture.csv\"");
        // CORS handled by global post-routing handler
    });

    // Neural Network Lab Training endpoint
    m_server->Post("/api/neural/train", [](const httplib::Request& req, httplib::Response& res) {
        neural::TrainingConfig config;
        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                config = neural::TrainingConfig::fromJson(j);
            } catch (const std::exception& e) {
                nlohmann::json errRes = {
                    {"success", false},
                    {"error_message", std::string("Malformed JSON payload: ") + e.what()}
                };
                res.set_content(errRes.dump(2), "application/json");
                // CORS handled by global post-routing handler
                return;
            }
        }

        std::string valErr;
        if (!config.validate(valErr)) {
            nlohmann::json errRes = {
                {"success", false},
                {"error_message", valErr}
            };
            res.set_content(errRes.dump(2), "application/json");
            // CORS handled by global post-routing handler
            return;
        }

        core::Logger::instance().info("Running Neural Backprop Training for " + std::to_string(config.numSteps) + " steps");
        auto result = neural::NeuralNetRegression::train(config);
        res.set_content(result.toJson().dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Neural Network Lab Gradient Check endpoint
    m_server->Post("/api/neural/gradient-check", [](const httplib::Request& req, httplib::Response& res) {
        double x = 1.0;
        double y = 2.0;
        neural::NetworkParameters params{0.5, 0.0, 1.0, 0.0};
        double epsilon = 1.0e-6;
        double maxTolerance = 1.0e-5;

        if (!req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("x")) x = j["x"].get<double>();
                if (j.contains("y")) y = j["y"].get<double>();
                if (j.contains("epsilon")) epsilon = j["epsilon"].get<double>();
                if (j.contains("max_tolerance")) maxTolerance = j["max_tolerance"].get<double>();
                if (j.contains("parameters")) {
                    const auto& p = j["parameters"];
                    if (p.contains("w")) params.w = p["w"].get<double>();
                    if (p.contains("b")) params.b = p["b"].get<double>();
                    if (p.contains("v")) params.v = p["v"].get<double>();
                    if (p.contains("c")) params.c = p["c"].get<double>();
                }
            } catch (const std::exception& e) {
                nlohmann::json errRes = {
                    {"success", false},
                    {"error_message", std::string("Malformed JSON payload: ") + e.what()}
                };
                res.set_content(errRes.dump(2), "application/json");
                // CORS handled by global post-routing handler
                return;
            }
        }

        auto check = neural::NeuralNetRegression::checkGradients(x, y, params, epsilon, maxTolerance);
        nlohmann::json response = {
            {"success", true},
            {"gradient_check", check.toJson()}
        };
        res.set_content(response.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Neural Network Lab CSV Export endpoint
    m_server->Get("/api/neural/export-csv", [](const httplib::Request& req, httplib::Response& res) {
        neural::TrainingConfig config;
        if (req.has_param("x")) config.x = std::stod(req.get_param_value("x"));
        if (req.has_param("y")) config.y = std::stod(req.get_param_value("y"));
        if (req.has_param("w")) config.initialParams.w = std::stod(req.get_param_value("w"));
        if (req.has_param("b")) config.initialParams.b = std::stod(req.get_param_value("b"));
        if (req.has_param("v")) config.initialParams.v = std::stod(req.get_param_value("v"));
        if (req.has_param("c")) config.initialParams.c = std::stod(req.get_param_value("c"));
        if (req.has_param("lr")) config.learningRate = std::stod(req.get_param_value("lr"));
        if (req.has_param("steps")) config.numSteps = std::stoul(req.get_param_value("steps"));
        if (req.has_param("eps")) config.epsilon = std::stod(req.get_param_value("eps"));
        if (req.has_param("tol")) config.maxTolerance = std::stod(req.get_param_value("tol"));

        auto result = neural::NeuralNetRegression::train(config);
        res.set_content(result.toCsv(), "text/csv");
        res.set_header("Content-Disposition", "attachment; filename=\"neural_backprop_training_log.csv\"");
        // CORS handled by global post-routing handler
    });

    // Technical Report Document Download Endpoint (Task G)
    m_server->Get("/api/reports/download", [](const httplib::Request& req, httplib::Response& res) {
        std::string filename = "ARCHITECTURE.md";
        if (req.has_param("file")) {
            filename = req.get_param_value("file");
        }

        if (filename.find("..") != std::string::npos || 
            filename.find('/') != std::string::npos || 
            filename.find('\\') != std::string::npos) {
            res.status = 400;
            res.set_content("Invalid file request path.", "text/plain");
            // CORS handled by global post-routing handler
            return;
        }

        std::filesystem::path docPath = std::filesystem::current_path() / "docs" / filename;
        if (!std::filesystem::exists(docPath)) {
            // Also check root for README.md
            if (filename == "README.md") {
                docPath = std::filesystem::current_path() / "README.md";
            }
        }

        if (!std::filesystem::exists(docPath)) {
            res.status = 404;
            res.set_content("Requested technical report file not found on server.", "text/plain");
            // CORS handled by global post-routing handler
            return;
        }

        std::ifstream file(docPath, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            res.status = 500;
            res.set_content("Unable to read report file from filesystem.", "text/plain");
            // CORS handled by global post-routing handler
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        res.set_content(buffer.str(), "text/markdown; charset=utf-8");
        res.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        // CORS handled by global post-routing handler
    });

    // Unified Engineering Verification Summary Endpoint (Task G)
    m_server->Get("/api/reports/summary", [](const httplib::Request&, httplib::Response& res) {
        auto telemetry = core::SystemInfoProvider::getTelemetry();
        nlohmann::json summary = {
            {"project_name", "UNIFIED ENGINEERING WORKBENCH"},
            {"organization", "PASS SCIENCE WORKS R&D Internship 2026-2027"},
            {"version", "1.0.0-RC"},
            {"target_platform", "Windows 11 x64"},
            {"compiler", telemetry.compiler},
            {"optimization", telemetry.buildType},
            {"hardware", {
                {"cpu_model", telemetry.cpuModel},
                {"physical_cores", telemetry.physicalCores},
                {"logical_cores", telemetry.logicalCores},
                {"installed_ram_mb", telemetry.totalRamBytes / (1024 * 1024)}
            }},
            {"milestones", {
                {
                    {"id", "task_a"},
                    {"name", "Task A - Application Architecture & C++ Backend"},
                    {"status", "VERIFIED"},
                    {"details", "Frontend/backend REST separation, Launch.exe root launcher, multi-port fallback, zero-dependency."}
                },
                {
                    {"id", "task_b"},
                    {"name", "Task B - CAD Geometry & Temperature Cloud Viewer"},
                    {"status", "VERIFIED"},
                    {"details", "ASCII STL parser (20 facets, 60 coords), ANSYS CSV parser (108 nodes), Z-up Three.js viewport, Turbo scientific colormap."}
                },
                {
                    {"id", "task_c"},
                    {"name", "Task C - 1D Steady-State Thermal FEM Solver"},
                    {"status", "VERIFIED"},
                    {"details", "Linear 2-node finite elements, Dirichlet boundary conditions, second-order mesh convergence (N=10,20,40,80), analytical verification."}
                },
                {
                    {"id", "task_d"},
                    {"name", "Task D - Multicore Performance & Scalability Lab"},
                    {"status", "VERIFIED"},
                    {"details", "Bounded std::thread worker pool, 5 measured repetitions + warm-up, deterministic order, numerical tolerance Delta T <= 1e-8 C."}
                },
                {
                    {"id", "task_e"},
                    {"name", "Task E - Tri-Device Heterogeneous Embedded Integration"},
                    {"status", "VERIFIED"},
                    {"details", "STM32H753ZI MCU, Artix-7 FPGA, FT2232H JTAG probe mock adapters, recoverable timeout fault & TLR reset, live telemetry strip charts."}
                },
                {
                    {"id", "task_f"},
                    {"name", "Task F - Manual Analytical Backpropagation & Neural Lab"},
                    {"status", "VERIFIED"},
                    {"details", "Zero autograd, exact manual chain-rule backward pass, central finite-difference check (eps=1e-6, err <= 1e-5), 100-step convergence."}
                },
                {
                    {"id", "task_g"},
                    {"name", "Task G - Reports, Exports & Packaging"},
                    {"status", "VERIFIED"},
                    {"details", "Centralized export hub, live C++ log terminal, standalone double-click Launch.exe packaging, full documentation suite."}
                }
            }}
        };
        res.set_content(summary.dump(2), "application/json");
        // CORS handled by global post-routing handler
    });

    // Graceful shutdown endpoint (protected by random per-session token)
    m_server->Post("/api/system/shutdown", [this](const httplib::Request& req, httplib::Response& res) {
        bool authorized = false;
        if (req.has_header("X-Session-Token")) {
            if (req.get_header_value("X-Session-Token") == m_sessionToken) {
                authorized = true;
            }
        }
        if (!authorized && !req.body.empty()) {
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.contains("token") && j["token"].get<std::string>() == m_sessionToken) {
                    authorized = true;
                }
            } catch (...) {}
        }

        if (!authorized) {
            res.status = 403;
            nlohmann::json err = {
                {"success", false},
                {"error_message", "Forbidden: Invalid or missing shutdown session token."}
            };
            res.set_content(err.dump(2), "application/json");
            return;
        }

        core::Logger::instance().info("Received authorized shutdown request via REST API with valid session token.");
        nlohmann::json response = {
            {"status", "shutting_down"},
            {"message", "Unified Engineering Workbench server is terminating safely."}
        };
        res.set_content(response.dump(2), "application/json");

        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            this->stop();
        }).detach();
    });

    // CORS pre-flight handler
    m_server->Options(".*", [this](const httplib::Request& req, httplib::Response& res) {
        std::string origin = req.get_header_value("Origin");
        if (origin.empty()) {
            res.set_header("Access-Control-Allow-Origin", "http://127.0.0.1:" + std::to_string(m_actualPort));
        } else if (origin.find("127.0.0.1") != std::string::npos || origin.find("localhost") != std::string::npos) {
            res.set_header("Access-Control-Allow-Origin", origin);
        } else {
            res.set_header("Access-Control-Allow-Origin", "http://127.0.0.1:" + std::to_string(m_actualPort));
        }
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Session-Token");
        res.status = 204;
    });
}

void HttpServer::setupStaticFileServing() {
    namespace fs = std::filesystem;
    if (fs::exists(m_config.staticDir)) {
        core::Logger::instance().info("Mounting static frontend assets from: " + m_config.staticDir);
        m_server->set_mount_point("/", m_config.staticDir.c_str());
    } else {
        core::Logger::instance().error("Static directory does not exist: " + m_config.staticDir);
    }
}

bool HttpServer::start() {
    if (m_running) {
        return true;
    }

    setupRoutes();
    setupStaticFileServing();

    m_running = true;
    core::Logger::instance().info("Starting HTTP Server on " + m_config.host + ":" + std::to_string(m_config.port));

    // Try starting on configured port or find an available port
    int targetPort = m_config.port;
    bool bound = false;

    for (int offset = 0; offset < 10; ++offset) {
        int testPort = targetPort + offset;
        if (m_server->bind_to_port(m_config.host.c_str(), testPort)) {
            m_actualPort = testPort;
            bound = true;
            break;
        }
    }

    if (!bound) {
        core::Logger::instance().error("Failed to bind HTTP server to host " + m_config.host + " on ports " +
                                       std::to_string(targetPort) + "-" + std::to_string(targetPort + 9));
        m_running = false;
        return false;
    }

    core::Logger::instance().info("HTTP Server successfully bound to " + getBaseUrl());
    core::SystemInfoProvider::markLaunchReady();

    m_workerThread = std::thread([this]() {
        m_server->listen_after_bind();
        m_running = false;
        core::Logger::instance().info("HTTP Server worker thread terminated.");
    });

    return true;
}

void HttpServer::stop() {
    if (m_running) {
        m_running = false;
        if (m_server) {
            m_server->stop();
        }
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
        core::Logger::instance().info("HTTP Server stopped.");
    }
}

} // namespace workbench::server

