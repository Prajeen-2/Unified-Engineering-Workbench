#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace workbench::core {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

struct LogEntry {
    std::string timestamp;
    LogLevel level;
    std::string message;
};

class Logger {
public:
    static Logger& instance();

    void log(LogLevel level, const std::string& message);
    void info(const std::string& message) { log(LogLevel::Info, message); }
    void warn(const std::string& message) { log(LogLevel::Warning, message); }
    void error(const std::string& message) { log(LogLevel::Error, message); }
    void debug(const std::string& message) { log(LogLevel::Debug, message); }

    std::vector<LogEntry> getRecentLogs(size_t maxCount = 100);
    void clear();

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string levelToString(LogLevel level) const;
    std::string currentTimestamp() const;

    std::mutex m_mutex;
    std::vector<LogEntry> m_logs;
};

} // namespace workbench::core
