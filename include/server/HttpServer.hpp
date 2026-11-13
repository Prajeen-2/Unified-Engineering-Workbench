#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>

namespace httplib {
    class Server;
}

namespace workbench::server {

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

private:
    void setupRoutes();
    void setupStaticFileServing();

    ServerConfig m_config;
    int m_actualPort{8080};
    std::unique_ptr<httplib::Server> m_server;
    std::atomic<bool> m_running{false};
    std::thread m_workerThread;
};

} // namespace workbench::server
