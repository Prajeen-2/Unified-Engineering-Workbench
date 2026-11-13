#include "core/Logger.hpp"

namespace workbench::core {

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

std::string Logger::currentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm timeinfo;
#if defined(_WIN32)
    localtime_s(&timeinfo, &in_time_t);
#else
    localtime_r(&in_time_t, &timeinfo);
#endif

    std::ostringstream ss;
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string ts = currentTimestamp();
    std::string lvlStr = levelToString(level);

    // Print to stdout
    std::cout << "[" << ts << "] [" << lvlStr << "] " << message << std::endl;

    m_logs.push_back({ts, level, message});
    if (m_logs.size() > 1000) {
        m_logs.erase(m_logs.begin(), m_logs.begin() + 100);
    }
}

std::vector<LogEntry> Logger::getRecentLogs(size_t maxCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logs.size() <= maxCount) {
        return m_logs;
    }
    return std::vector<LogEntry>(m_logs.end() - maxCount, m_logs.end());
}

void Logger::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logs.clear();
}

} // namespace workbench::core
