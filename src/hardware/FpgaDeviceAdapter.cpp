#include "hardware/FpgaDeviceAdapter.hpp"
#include <cmath>

namespace workbench::hardware {

FpgaDeviceAdapter::FpgaDeviceAdapter() {
    m_state = DeviceState::Disconnected;
}

DeviceState FpgaDeviceAdapter::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

DeviceCapability FpgaDeviceAdapter::getCapabilities() const {
    DeviceCapability cap;
    cap.deviceId = getDeviceId();
    cap.deviceName = getDeviceName();
    cap.type = DeviceType::FPGA;
    cap.architecture = "Xilinx 7-Series Artix-7 (XC7A100T-2FTG256C)";
    cap.hostInterface = "FTDI FT232H Synchronous FIFO / SPI / JTAG";
    cap.voltageDomains = {"1.0V VCCINT Core", "1.8V VCCAUX Auxiliary", "3.3V VCCO Bank 0", "1.8V VCCO Bank 1"};
    cap.maxClockMHz = 450;
    cap.supportedProtocols = {"AXI4-Stream", "SPI Master/Slave", "UART (12 Mbaud)", "IEEE 1149.1 JTAG (Boundary Scan)"};
    cap.supportsHotPlug = true;
    cap.supportsFaultInjection = true;
    return cap;
}

bool FpgaDeviceAdapter::connect(uint32_t timeoutMs) {
    (void)timeoutMs;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = DeviceState::Connected;
    m_connectTime = std::chrono::steady_clock::now();
    m_faultInjected = false;
    m_lastFaultReason.clear();
    m_packetsTx = 48;
    m_packetsRx = 48;
    m_crcErrors = 0;
    return true;
}

bool FpgaDeviceAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = DeviceState::Disconnected;
    m_faultInjected = false;
    return true;
}

DeviceTelemetry FpgaDeviceAdapter::pollTelemetry() {
    std::lock_guard<std::mutex> lock(m_mutex);
    DeviceTelemetry telem;
    telem.deviceId = getDeviceId();
    telem.state = m_state;

    if (m_state == DeviceState::Disconnected) {
        telem.lastLogMessage = "FPGA fabric link is offline. Device unconfigured.";
        return telem;
    }

    if (m_faultInjected) {
        telem.state = DeviceState::ErrorTimeout;
        telem.lastLogMessage = "[TIMEOUT ERROR] FPGA synchronous FIFO handshake timed out (NO_ACK on pin DONE).";
        telem.crcErrors++;
        return telem;
    }

    m_state = DeviceState::Streaming;
    telem.state = DeviceState::Streaming;

    auto now = std::chrono::steady_clock::now();
    telem.uptimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_connectTime).count();

    m_packetsTx += 32;
    m_packetsRx += 32;
    telem.packetsTransmitted = m_packetsTx;
    telem.packetsReceived = m_packetsRx;
    telem.crcErrors = m_crcErrors;

    std::uniform_real_distribution<double> distV10(-0.004, 0.004);
    std::uniform_real_distribution<double> distV18(-0.008, 0.008);
    std::uniform_real_distribution<double> distV33(-0.012, 0.012);
    std::uniform_real_distribution<double> distTemp(-0.4, 0.4);
    std::uniform_real_distribution<double> distCurr(-8.0, 8.0);

    telem.railVoltages.push_back({"1.0V_VCCINT", 1.002 + distV10(m_rng)});
    telem.railVoltages.push_back({"1.8V_VCCAUX", 1.805 + distV18(m_rng)});
    telem.railVoltages.push_back({"3.3V_VCCO_0", 3.312 + distV33(m_rng)});

    telem.coreTemperatureC = 43.6 + distTemp(m_rng);
    telem.currentDrawMa = 455.0 + distCurr(m_rng);

    telem.deviceSpecificData = {
        {"lut_utilization_percent", 38.4},
        {"bram_blocks_used", "42 / 135 (31.1%)"},
        {"dsp_slices_used", "18 / 240 (7.5%)"},
        {"mmcm_pll_status", "LOCKED @ 100.000 MHz"},
        {"bitstream_crc", "VALID (0x98A1B2C4)"},
        {"xadc_vaux0_volts", 0.985 + distV10(m_rng)}
    };

    telem.lastLogMessage = "FPGA AXI4-Stream telemetry packet verified (CRC-32 OK).";
    return telem;
}

bool FpgaDeviceAdapter::injectFault(const std::string& faultType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_faultInjected = true;
    m_lastFaultReason = faultType.empty() ? "timeout" : faultType;
    m_state = DeviceState::ErrorTimeout;
    return true;
}

bool FpgaDeviceAdapter::recoverFault() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_faultInjected && m_state != DeviceState::ErrorTimeout) {
        return false;
    }
    // Simulate FPGA bitstream warm reconfiguration & FIFO re-alignment
    m_faultInjected = false;
    m_lastFaultReason.clear();
    m_state = DeviceState::Connected;
    return true;
}

} // namespace workbench::hardware

