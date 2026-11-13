#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include "nlohmann/json.hpp"

namespace httplib {
    class Server;
}

namespace workbench::server {

struct AsyncJob {
    std::string id;
    std::string type;
    std::string status{"running"}; // "running", "completed", "cancelled", "failed"
    int progressPercent{0};
    std::string message;
    std::atomic<bool> cancelRequested{false};
    nlohmann::json result;
    std::string errorMessage;
};

struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string staticDir = "./frontend";
};

class HttpServer {
public:
    explicit HttpServer(const ServerConfig& config = ServerConfig{});
    ~HttpServer();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    int getPort() const { return m_actualPort; }
    std::string getHost() const { return m_config.host; }
    std::string getBaseUrl() const;
    const std::string& getSessionToken() const { return m_sessionToken; }

private:
    void setupRoutes();
    void setupStaticFileServing();

    ServerConfig m_config;
    int m_actualPort{8080};
    std::unique_ptr<httplib::Server> m_server;
    std::atomic<bool> m_running{false};
    std::thread m_workerThread;

    std::string m_sessionToken;
    std::mutex m_jobsMutex;
    std::unordered_map<std::string, std::shared_ptr<AsyncJob>> m_jobs;
    std::atomic<uint64_t> m_jobCounter{0};
};

} // namespace workbench::server

