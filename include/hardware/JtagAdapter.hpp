#pragma once

#include "hardware/IHardwareDevice.hpp"
#include <mutex>
#include <random>

namespace workbench::hardware {

class JtagAdapter : public IHardwareDevice {
public:
    JtagAdapter();
    ~JtagAdapter() override = default;

    std::string getDeviceId() const override { return "jtag_ft2232h"; }
    std::string getDeviceName() const override { return "FT2232H High-Speed USB JTAG Adapter"; }
    DeviceType getDeviceType() const override { return DeviceType::JtagAdapter; }
    DeviceState getState() const override;
    DeviceCapability getCapabilities() const override;

    bool connect(uint32_t timeoutMs = 1000) override;
    bool disconnect() override;
    DeviceTelemetry pollTelemetry() override;

    bool injectFault(const std::string& faultType) override;
    bool recoverFault() override;

private:
    mutable std::mutex m_mutex;
    DeviceState m_state{DeviceState::Disconnected};
    std::chrono::steady_clock::time_point m_connectTime;
    uint64_t m_shiftsExecuted{0};
    uint32_t m_crcErrors{0};
    bool m_faultInjected{false};
    std::string m_lastFaultReason;
    std::string m_currentTapState{"Test-Logic-Reset"};

    std::mt19937 m_rng{303};
};

} // namespace workbench::hardware

