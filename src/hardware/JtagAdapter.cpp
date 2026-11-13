#include "hardware/JtagAdapter.hpp"
#include <cmath>

namespace workbench::hardware {

JtagAdapter::JtagAdapter() {
    m_state = DeviceState::Disconnected;
}

DeviceState JtagAdapter::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

DeviceCapability JtagAdapter::getCapabilities() const {
    DeviceCapability cap;
    cap.deviceId = getDeviceId();
    cap.deviceName = getDeviceName();
    cap.type = DeviceType::JtagAdapter;
    cap.architecture = "FTDI FT2232H Dual High-Speed USB UART/FIFO / MPSSE Engine";
    cap.hostInterface = "USB 2.0 High-Speed 480 Mbps (Dedicated WinUSB endpoint)";
    cap.voltageDomains = {"3.3V VCCIO", "VTarget Sense Input (1.2V - 3.6V tolerant)"};
    cap.maxClockMHz = 30; // Max TCK clock frequency
    cap.supportedProtocols = {"IEEE 1149.1 JTAG", "ARM SWD", "SPI / I2C via MPSSE"};
    cap.supportsHotPlug = true;
    cap.supportsFaultInjection = true;
    return cap;
}

bool JtagAdapter::connect(uint32_t timeoutMs) {
    (void)timeoutMs;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = DeviceState::Connected;
    m_connectTime = std::chrono::steady_clock::now();
    m_faultInjected = false;
    m_lastFaultReason.clear();
    m_shiftsExecuted = 256;
    m_crcErrors = 0;
    m_currentTapState = "Run-Test/Idle";
    return true;
}

bool JtagAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = DeviceState::Disconnected;
    m_faultInjected = false;
    m_currentTapState = "Test-Logic-Reset";
    return true;
}

DeviceTelemetry JtagAdapter::pollTelemetry() {
    std::lock_guard<std::mutex> lock(m_mutex);
    DeviceTelemetry telem;
    telem.deviceId = getDeviceId();
    telem.state = m_state;

    if (m_state == DeviceState::Disconnected) {
        telem.lastLogMessage = "JTAG probe detached. Port High-Z.";
        return telem;
    }

    if (m_faultInjected) {
        telem.state = DeviceState::ErrorTimeout;
        telem.lastLogMessage = "[TIMEOUT ERROR] USB-MPSSE transaction timed out waiting for TDO response. Peripheral communication bus stalled.";
        telem.crcErrors++;
        return telem;
    }

    m_state = DeviceState::Streaming;
    telem.state = DeviceState::Streaming;

    auto now = std::chrono::steady_clock::now();
    telem.uptimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_connectTime).count();

    m_shiftsExecuted += 128;
    telem.packetsTransmitted = m_shiftsExecuted;
    telem.packetsReceived = m_shiftsExecuted;
    telem.crcErrors = m_crcErrors;

    std::uniform_real_distribution<double> distVolt(-0.008, 0.008);
    std::uniform_real_distribution<double> distCurr(-1.0, 1.0);

    double vTarget = 3.288 + distVolt(m_rng);
    telem.railVoltages.push_back({"VTarget_Sense", vTarget});
    telem.railVoltages.push_back({"3.3V_USB_VBUS", 4.985 + distVolt(m_rng)});

    telem.coreTemperatureC = 35.10;
    telem.currentDrawMa = 68.0 + distCurr(m_rng);

    // Multi-drop scan chain telemetry
    telem.deviceSpecificData = {
        {"tck_frequency_mhz", 15.0},
        {"tap_controller_state", "Shift-DR / Run-Test/Idle"},
        {"scan_chain_length", 2},
        {"devices_in_chain", {
            {
                {"tap_index", 0},
                {"device", "STM32H753ZI (ARM Cortex-M7)"},
                {"idcode", "0x6BA02477"},
                {"ir_length_bits", 4},
                {"status", "BYPASS / ACTIVE"}
            },
            {
                {"tap_index", 1},
                {"device", "Xilinx Artix-7 (XC7A100T)"},
                {"idcode", "0x0362D093"},
                {"ir_length_bits", 6},
                {"status", "BYPASS / ACTIVE"}
            }
        }},
        {"total_ir_length_bits", 10},
        {"bit_slip_errors", 0}
    };

    telem.lastLogMessage = "JTAG MPSSE scan chain verified (2 TAPs discovered: IDCODE 0x6BA02477 & 0x0362D093). Bus active.";
    return telem;
}

bool JtagAdapter::injectFault(const std::string& faultType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_faultInjected = true;
    m_lastFaultReason = faultType.empty() ? "timeout" : faultType;
    m_state = DeviceState::ErrorTimeout;
    return true;
}

bool JtagAdapter::recoverFault() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_faultInjected && m_state != DeviceState::ErrorTimeout) {
        return false;
    }
    // Note: Standard IEEE 1149.1 Test-Logic-Reset (TLR) resets only the TAP controller state machine.
    // To recover from a hanging peripheral communication timeout, the MPSSE bus controller performs a
    // hardware bus reset, FIFO buffer flush, link renegotiation, and session reconnect/retry sequence.
    m_faultInjected = false;
    m_lastFaultReason.clear();
    m_currentTapState = "Run-Test/Idle";
    m_state = DeviceState::Connected;
    return true;
}

} // namespace workbench::hardware

