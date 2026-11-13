#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include "core/Logger.hpp"
#include "core/SystemInfo.hpp"
#include "server/HttpServer.hpp"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

namespace {
    std::atomic<bool> g_keepRunning{true};

    void signalHandler(int) {
        g_keepRunning = false;
    }

    void openBrowser(const std::string& url) {
#if defined(_WIN32)
        std::wstring wUrl(url.begin(), url.end());
        ShellExecuteW(NULL, L"open", wUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
        std::string cmd = "xdg-open " + url;
        (void)system(cmd.c_str());
#endif
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto& logger = workbench::core::Logger::instance();
    logger.info("=======================================================");
    logger.info("   PASS SCIENCE WORKS - UNIFIED ENGINEERING WORKBENCH   ");
    logger.info("       R&D Internship 2026-2027 Submission Engine       ");
    logger.info("=======================================================");

    int port = 8080;
    std::string host = "127.0.0.1";
    bool launchBrowser = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            std::string portArg = argv[++i];
            try {
                size_t pos = 0;
                int parsed = std::stoi(portArg, &pos);
                if (pos == portArg.size() && parsed > 0 && parsed <= 65535) {
                    port = parsed;
                } else {
                    logger.error("Invalid port specified: '" + portArg + "'. Using default 8080.");
                }
            } catch (const std::exception&) {
                logger.error("Non-numeric port argument: '" + portArg + "'. Using default 8080.");
            }
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--no-browser") {
            launchBrowser = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: workbench_server [options]\n"
                      << "Options:\n"
                      << "  --port <number>   Specify HTTP server port (default: 8080)\n"
                      << "  --host <ip>       Specify host address (default: 127.0.0.1)\n"
                      << "  --no-browser      Do not automatically open default web browser\n"
                      << "  --help, -h        Show this help message\n";
            return 0;
        }
    }

    auto telemetry = workbench::core::SystemInfoProvider::getTelemetry();
    logger.info("Host Architecture: " + telemetry.architecture + " | OS: " + telemetry.osName);
    logger.info("CPU Processor:     " + telemetry.cpuModel);
    logger.info("Physical Cores:    " + std::to_string(telemetry.physicalCores) +
                " | Logical Cores: " + std::to_string(telemetry.logicalCores));
    logger.info("Installed RAM:     " + std::to_string(telemetry.totalRamBytes / (1024 * 1024)) + " MB");
    logger.info("Compiled with:     " + telemetry.compiler);
    logger.info("Optimization:      " + telemetry.buildType);

    std::string frontendPath = workbench::core::SystemInfoProvider::resolveFrontendPath();
    logger.info("Resolved Frontend UI Root: " + frontendPath);

    workbench::server::ServerConfig config;
    config.host = host;
    config.port = port;
    config.staticDir = frontendPath;

    workbench::server::HttpServer server(config);
    if (!server.start()) {
        logger.error("Fatal: Could not initialize backend HTTP server. Exiting.");
        return 1;
    }

    std::string appUrl = server.getBaseUrl();
    logger.info("Unified Workbench ready at: " + appUrl);
    logger.info("Press Ctrl+C or type 'q' and press Enter to terminate server.");

    if (launchBrowser) {
        logger.info("Launching default web browser to: " + appUrl);
        openBrowser(appUrl);
    }

    // Monitor loop
    while (g_keepRunning && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    logger.info("Shutting down Unified Engineering Workbench backend...");
    server.stop();
    logger.info("Server terminated cleanly. Goodbye.");
    return 0;
}

