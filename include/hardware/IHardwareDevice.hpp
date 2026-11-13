#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <utility>
#include "nlohmann/json.hpp"

namespace workbench::hardware {

enum class DeviceType {
    Microcontroller,
    FPGA,
    JtagAdapter
};

enum class DeviceState {
    Disconnected,
    Connecting,
    Connected,
    Streaming,
    ErrorTimeout,
    Recovering
};

inline std::string deviceTypeToString(DeviceType t) {
    switch (t) {
        case DeviceType::Microcontroller: return "Microcontroller";
        case DeviceType::FPGA:            return "FPGA";
        case DeviceType::JtagAdapter:     return "JTAG Debug Adapter";
    }
    return "Unknown";
}

inline std::string deviceStateToString(DeviceState s) {
    switch (s) {
        case DeviceState::Disconnected: return "DISCONNECTED";
        case DeviceState::Connecting:   return "CONNECTING";
        case DeviceState::Connected:    return "CONNECTED";
        case DeviceState::Streaming:    return "STREAMING";
        case DeviceState::ErrorTimeout: return "ERROR_TIMEOUT";
        case DeviceState::Recovering:   return "RECOVERING";
    }
    return "UNKNOWN";
}

struct DeviceCapability {
    std::string deviceId;
    std::string deviceName;
    DeviceType type{DeviceType::Microcontroller};
    std::string architecture;
    std::string hostInterface;
    std::vector<std::string> voltageDomains;
    uint32_t maxClockMHz{0};
    std::vector<std::string> supportedProtocols;
    bool supportsHotPlug{true};
    bool supportsFaultInjection{true};

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"device_id", deviceId},
            {"device_name", deviceName},
            {"device_type", deviceTypeToString(type)},
            {"architecture", architecture},
            {"host_interface", hostInterface},
            {"voltage_domains", voltageDomains},
            {"max_clock_mhz", maxClockMHz},
            {"supported_protocols", supportedProtocols},
            {"supports_hot_plug", supportsHotPlug},
            {"supports_fault_injection", supportsFaultInjection}
        };
    }
};

struct DeviceTelemetry {
    std::string deviceId;
    DeviceState state{DeviceState::Disconnected};
    uint64_t uptimeMs{0};
    uint64_t packetsTransmitted{0};
    uint64_t packetsReceived{0};
    uint32_t crcErrors{0};
    double coreTemperatureC{0.0};
    std::vector<std::pair<std::string, double>> railVoltages;
    double currentDrawMa{0.0};
    std::string lastLogMessage;
    nlohmann::json deviceSpecificData;

    nlohmann::json toJson() const {
        nlohmann::json rails = nlohmann::json::object();
        for (const auto& rail : railVoltages) {
            rails[rail.first] = std::round(rail.second * 1000.0) / 1000.0;
        }

        return nlohmann::json{
            {"device_id", deviceId},
            {"state", deviceStateToString(state)},
            {"uptime_ms", uptimeMs},
            {"packets_transmitted", packetsTransmitted},
            {"packets_received", packetsReceived},
            {"crc_errors", crcErrors},
            {"core_temperature_c", std::round(coreTemperatureC * 100.0) / 100.0},
            {"rail_voltages", rails},
            {"current_draw_ma", std::round(currentDrawMa * 10.0) / 10.0},
            {"last_log_message", lastLogMessage},
            {"device_specific", deviceSpecificData}
        };
    }
};

class IHardwareDevice {
public:
    virtual ~IHardwareDevice() = default;

    virtual std::string getDeviceId() const = 0;
    virtual std::string getDeviceName() const = 0;
    virtual DeviceType getDeviceType() const = 0;
    virtual DeviceState getState() const = 0;
    virtual DeviceCapability getCapabilities() const = 0;

    virtual bool connect(uint32_t timeoutMs = 1000) = 0;
    virtual bool disconnect() = 0;
    virtual DeviceTelemetry pollTelemetry() = 0;

    // Recoverable failure simulation (Task E mandate)
    virtual bool injectFault(const std::string& faultType) = 0;
    virtual bool recoverFault() = 0;

    // Simulation distinction
    virtual bool isSimulated() const { return true; }
};

} // namespace workbench::hardware
