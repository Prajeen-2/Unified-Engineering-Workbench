#pragma once

#include "hardware/IHardwareDevice.hpp"
#include <mutex>
#include <random>

namespace workbench::hardware {

class FpgaDeviceAdapter : public IHardwareDevice {
public:
    FpgaDeviceAdapter();
    ~FpgaDeviceAdapter() override = default;

    std::string getDeviceId() const override { return "fpga_artix7"; }
    std::string getDeviceName() const override { return "Xilinx Artix-7 XC7A100T FPGA"; }
    DeviceType getDeviceType() const override { return DeviceType::FPGA; }
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
    uint64_t m_packetsTx{0};
    uint64_t m_packetsRx{0};
    uint32_t m_crcErrors{0};
    bool m_faultInjected{false};
    std::string m_lastFaultReason;

    std::mt19937 m_rng{202};
};

} // namespace workbench::hardware
