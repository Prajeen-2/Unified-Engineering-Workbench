#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "server/HttpServer.hpp"
#include "core/Logger.hpp"
#include "core/SystemInfo.hpp"
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
