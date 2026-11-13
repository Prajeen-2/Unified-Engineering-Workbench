#include "hardware/McuDeviceAdapter.hpp"
#include <cmath>

namespace workbench::hardware {

McuDeviceAdapter::McuDeviceAdapter() {
    m_state = DeviceState::Disconnected;
}

DeviceState McuDeviceAdapter::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

DeviceCapability McuDeviceAdapter::getCapabilities() const {
    DeviceCapability cap;
    cap.deviceId = getDeviceId();
    cap.deviceName = getDeviceName();
    cap.type = DeviceType::Microcontroller;
    cap.architecture = "ARM Cortex-M7 (ARMv7E-M with DP-FPU)";
    cap.hostInterface = "USB 2.0 Full-Speed CDC-ACM Virtual COM Port (115,200 baud)";
    cap.voltageDomains = {"3.3V VDD Digital/Analog", "1.2V VCORE Internal LDO"};
    cap.maxClockMHz = 480;
    cap.supportedProtocols = {"UART / USART", "SPI (Full-Duplex)", "I2C (Fast-Mode Plus)", "CAN-FD", "JTAG / SWD"};
    cap.supportsHotPlug = true;
    cap.supportsFaultInjection = true;
    return cap;
}

bool McuDeviceAdapter::connect(uint32_t timeoutMs) {
    (void)timeoutMs;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = DeviceState::Connected;
    m_connectTime = std::chrono::steady_clock::now();
    m_faultInjected = false;
    m_lastFaultReason.clear();
    m_packetsTx = 14;
    m_packetsRx = 14;
    m_crcErrors = 0;
    m_heartbeat = 1;
    return true;
}

bool McuDeviceAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = DeviceState::Disconnected;
    m_faultInjected = false;
    return true;
}

DeviceTelemetry McuDeviceAdapter::pollTelemetry() {
    std::lock_guard<std::mutex> lock(m_mutex);
    DeviceTelemetry telem;
    telem.deviceId = getDeviceId();
    telem.state = m_state;

    if (m_state == DeviceState::Disconnected) {
        telem.lastLogMessage = "MCU link is offline. Device disconnected.";
        return telem;
    }

    if (m_faultInjected) {
        telem.state = DeviceState::ErrorTimeout;
        telem.lastLogMessage = "[TIMEOUT ERROR] MCU response timed out after 1000 ms. USART ACK frame lost.";
        telem.crcErrors++;
        return telem;
    }

    m_state = DeviceState::Streaming;
    telem.state = DeviceState::Streaming;

    auto now = std::chrono::steady_clock::now();
    telem.uptimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_connectTime).count();

    m_packetsTx += 8;
    m_packetsRx += 8;
    m_heartbeat++;
    telem.packetsTransmitted = m_packetsTx;
    telem.packetsReceived = m_packetsRx;
    telem.crcErrors = m_crcErrors;

    // Realistic Gaussian-distributed analog noise
    std::uniform_real_distribution<double> distNoise(-0.015, 0.015);
    std::uniform_real_distribution<double> distTempNoise(-0.35, 0.35);
    std::uniform_real_distribution<double> distCurrNoise(-2.5, 2.5);

    double vdd33 = 3.305 + distNoise(m_rng);
    double coreTemp = 38.20 + distTempNoise(m_rng);
    double currMa = 124.5 + distCurrNoise(m_rng);

    telem.railVoltages.push_back({"3.3V_VDD", vdd33});
    telem.coreTemperatureC = coreTemp;
    telem.currentDrawMa = currMa;

    // Extra microcontroller diagnostics
    telem.deviceSpecificData = {
        {"heartbeat_tick", m_heartbeat},
        {"freertos_heap_free_kb", 192.4},
        {"adc_channel_1_volts", 1.652 + distNoise(m_rng)},
        {"adc_channel_2_volts", 2.481 + distNoise(m_rng)},
        {"cpu_load_percent", 18.5},
        {"watchdog_status", "HEALTHY_ACTIVE"}
    };

    telem.lastLogMessage = "MCU Telemetry frame parsed successfully (CRC-16 OK).";
    return telem;
}

bool McuDeviceAdapter::injectFault(const std::string& faultType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_faultInjected = true;
    m_lastFaultReason = faultType.empty() ? "timeout" : faultType;
    m_state = DeviceState::ErrorTimeout;
    return true;
}

bool McuDeviceAdapter::recoverFault() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_faultInjected && m_state != DeviceState::ErrorTimeout) {
        return false;
    }
    // Simulate soft watchdog reset & re-handshake
    m_faultInjected = false;
    m_lastFaultReason.clear();
    m_state = DeviceState::Connected;
    return true;
}

} // namespace workbench::hardware
