#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
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

namespace workbench::server {

HttpServer::HttpServer(const ServerConfig& config)
    : m_config(config),
      m_actualPort(config.port),
      m_server(std::make_unique<httplib::Server>()) {
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
    // Health check endpoint
    m_server->Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json response = {
            {"status", "ok"},
            {"service", "Unified Engineering Workbench Backend"},
            {"version", "1.0.0"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        res.set_content(response.dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // System telemetry endpoint
    m_server->Get("/api/system/info", [](const httplib::Request&, httplib::Response& res) {
        auto telemetry = core::SystemInfoProvider::getTelemetry();
        res.set_content(telemetry.toJson().dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // Sample Benchmark Geometry Assets
    m_server->Get("/api/geometry/sample-stl", [](const httplib::Request&, httplib::Response& res) {
        std::string stlPath = core::SystemInfoProvider::resolvePath("data/stl/engineering_bracket.stl");
        auto parseRes = geometry::StlParser::parseAsciiFile(stlPath);
        res.set_content(parseRes.toJson().dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    m_server->Get("/api/geometry/sample-csv", [](const httplib::Request&, httplib::Response& res) {
        std::string csvPath = core::SystemInfoProvider::resolvePath("data/csv/synthetic_thermal_field.csv");
        auto parseRes = geometry::CsvParser::parseFile(csvPath);
        res.set_content(parseRes.toJson().dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
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
                    res.set_header("Access-Control-Allow-Origin", "*");
                    return;
                }
            } catch (...) {
                contentToParse = req.body;
            }
        }

        auto parseRes = geometry::StlParser::parseAsciiString(contentToParse);
        res.set_content(parseRes.toJson().dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
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
                    res.set_header("Access-Control-Allow-Origin", "*");
                    return;
                }
            } catch (...) {
                contentToParse = req.body;
            }
        }

        auto parseRes = geometry::CsvParser::parseString(contentToParse);
        res.set_content(parseRes.toJson().dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
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
                res.set_header("Access-Control-Allow-Origin", "*");
                return;
            }
        }

        auto solveRes = thermal::ThermalSolver1D::solve(params);
        res.set_content(solveRes.toJson().dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
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
                res.set_header("Access-Control-Allow-Origin", "*");
                return;
            }
        }

        auto studyRes = thermal::ThermalSolver1D::runConvergenceStudy(params, meshSizes);
        res.set_content(studyRes.toJson().dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
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
                res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    m_server->Get("/api/hardware/telemetry", [](const httplib::Request&, httplib::Response& res) {
        auto telem = hardware::HardwareManager::instance().getAllTelemetry();
        res.set_content(telem.dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    m_server->Get("/api/hardware/architecture", [](const httplib::Request&, httplib::Response& res) {
        auto arch = hardware::HardwareManager::getSystemElectricalArchitecture();
        res.set_content(arch.dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
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
                res.set_header("Access-Control-Allow-Origin", "*");
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
            res.set_header("Access-Control-Allow-Origin", "*");
            return;
        }

        core::Logger::instance().info("Running Neural Backprop Training for " + std::to_string(config.numSteps) + " steps");
        auto result = neural::NeuralNetRegression::train(config);
        res.set_content(result.toJson().dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
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
                res.set_header("Access-Control-Allow-Origin", "*");
                return;
            }
        }

        auto check = neural::NeuralNetRegression::checkGradients(x, y, params, epsilon, maxTolerance);
        nlohmann::json response = {
            {"success", true},
            {"gradient_check", check.toJson()}
        };
        res.set_content(response.dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
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
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // Graceful shutdown endpoint
    m_server->Post("/api/system/shutdown", [this](const httplib::Request&, httplib::Response& res) {
        core::Logger::instance().info("Received shutdown request via REST API.");
        nlohmann::json response = {
            {"status", "shutting_down"},
            {"message", "Unified Engineering Workbench server is terminating safely."}
        };
        res.set_content(response.dump(2), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");

        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            this->stop();
        }).detach();
    });

    // CORS pre-flight handler
    m_server->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
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

