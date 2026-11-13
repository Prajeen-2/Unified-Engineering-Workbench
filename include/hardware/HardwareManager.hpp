#pragma once

#include "hardware/IHardwareDevice.hpp"
#include "hardware/McuDeviceAdapter.hpp"
#include "hardware/FpgaDeviceAdapter.hpp"
#include "hardware/JtagAdapter.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>
#include "nlohmann/json.hpp"

namespace workbench::hardware {

class HardwareManager {
public:
    static HardwareManager& instance();

    std::vector<DeviceCapability> getDiscoveredDevices() const;
    IHardwareDevice* getDevice(const std::string& deviceId);

    bool connectDevice(const std::string& deviceId);
    bool disconnectDevice(const std::string& deviceId);
    bool connectAll();
    bool disconnectAll();

    nlohmann::json getAllTelemetry();

    bool injectFault(const std::string& deviceId, const std::string& faultType = "timeout");
    bool recoverFault(const std::string& deviceId);

    // Register write/read across targets
    bool writeRegister(const std::string& deviceId, uint32_t address, uint32_t value);
    bool readRegister(const std::string& deviceId, uint32_t address, uint32_t& outValue);

    // JTAG loopback verification
    bool verifyJtagLoopback(const std::vector<uint8_t>& pattern, std::vector<uint8_t>& outEcho);

    // Structural Electrical & System Architecture Metadata
    static nlohmann::json getSystemElectricalArchitecture();

    // Disable copy/move
    HardwareManager(const HardwareManager&) = delete;
    HardwareManager& operator=(const HardwareManager&) = delete;

private:
    HardwareManager();
    ~HardwareManager() = default;

    mutable std::mutex m_managerMutex;
    std::unordered_map<std::string, std::unique_ptr<IHardwareDevice>> m_devices;
};

} // namespace workbench::hardware

