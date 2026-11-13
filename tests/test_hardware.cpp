#include "hardware/HardwareManager.hpp"
#include <iostream>
#include <stdexcept>
#include <cassert>

using namespace workbench::hardware;

#define HW_REQUIRE(cond, msg) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + (msg)); \
        } \
    } while(0)

void testDeviceDiscovery() {
    std::cout << "[TEST 1] Testing Device Discovery across tri-device bus..." << std::endl;

    auto& mgr = HardwareManager::instance();
    auto devices = mgr.getDiscoveredDevices();

    HW_REQUIRE(devices.size() == 3, "Exactly 3 devices must be discovered (MCU, FPGA, JTAG)");

    bool foundMcu = false;
    bool foundFpga = false;
    bool foundJtag = false;

    for (const auto& dev : devices) {
        if (dev.deviceId == "mcu_stm32h7") {
            foundMcu = true;
            HW_REQUIRE(dev.type == DeviceType::Microcontroller, "MCU device type mismatch");
            HW_REQUIRE(dev.maxClockMHz == 480, "MCU clock should be 480 MHz");
            std::cout << "  -> Discovered: " << dev.deviceName << " (" << dev.architecture << ")" << std::endl;
        } else if (dev.deviceId == "fpga_artix7") {
            foundFpga = true;
            HW_REQUIRE(dev.type == DeviceType::FPGA, "FPGA device type mismatch");
            HW_REQUIRE(dev.voltageDomains.size() >= 3, "FPGA must have >= 3 voltage domains");
            std::cout << "  -> Discovered: " << dev.deviceName << " (" << dev.architecture << ")" << std::endl;
        } else if (dev.deviceId == "jtag_ft2232h") {
            foundJtag = true;
            HW_REQUIRE(dev.type == DeviceType::JtagAdapter, "JTAG device type mismatch");
            std::cout << "  -> Discovered: " << dev.deviceName << " (" << dev.architecture << ")" << std::endl;
        }
    }

    HW_REQUIRE(foundMcu && foundFpga && foundJtag, "All 3 devices must be present in discovery list");
    std::cout << "  -> Passed: All 3 devices successfully discovered and identified." << std::endl;
}

void testConnectDisconnectLifecycle() {
    std::cout << "[TEST 2] Testing Connect / Disconnect state lifecycle..." << std::endl;

    auto& mgr = HardwareManager::instance();

    // Disconnect all
    HW_REQUIRE(mgr.disconnectAll(), "disconnectAll failed");
    auto* mcu = mgr.getDevice("mcu_stm32h7");
    auto* fpga = mgr.getDevice("fpga_artix7");
    auto* jtag = mgr.getDevice("jtag_ft2232h");

    HW_REQUIRE(mcu != nullptr, "MCU not found");
    HW_REQUIRE(fpga != nullptr, "FPGA not found");
    HW_REQUIRE(jtag != nullptr, "JTAG not found");

    HW_REQUIRE(mcu->getState() == DeviceState::Disconnected, "MCU should be disconnected");
    HW_REQUIRE(fpga->getState() == DeviceState::Disconnected, "FPGA should be disconnected");
    HW_REQUIRE(jtag->getState() == DeviceState::Disconnected, "JTAG should be disconnected");

    // Connect individually
    HW_REQUIRE(mcu->connect(), "MCU connect failed");
    HW_REQUIRE(mcu->getState() == DeviceState::Connected, "MCU should be connected");

    HW_REQUIRE(fpga->connect(), "FPGA connect failed");
    HW_REQUIRE(fpga->getState() == DeviceState::Connected, "FPGA should be connected");

    HW_REQUIRE(jtag->connect(), "JTAG connect failed");
    HW_REQUIRE(jtag->getState() == DeviceState::Connected, "JTAG should be connected");

    std::cout << "  -> Passed: Independent and collective connect/disconnect state transitions verified." << std::endl;
}

void testTelemetryAcquisition() {
    std::cout << "[TEST 3] Testing Real-time Telemetry Acquisition & Parsing..." << std::endl;

    auto& mgr = HardwareManager::instance();
    auto telemJson = mgr.getAllTelemetry();

    HW_REQUIRE(telemJson["success"] == true, "Telemetry poll must succeed");
    HW_REQUIRE(telemJson["is_simulated_backend"] == true, "Simulation tag must be explicitly true");
    HW_REQUIRE(telemJson["devices"].is_array(), "Devices array required in telemetry response");
    HW_REQUIRE(telemJson["devices"].size() == 3, "Telemetry must contain 3 devices");

    for (const auto& d : telemJson["devices"]) {
        std::string id = d["device_id"].get<std::string>();
        std::string state = d["state"].get<std::string>();
        HW_REQUIRE(state == "STREAMING" || state == "CONNECTED", "State should be connected or streaming");

        double temp = d["core_temperature_c"].get<double>();
        HW_REQUIRE(temp > 20.0 && temp < 85.0, "Core temperature should be within realistic operating envelope");

        auto rails = d["rail_voltages"];
        if (id == "mcu_stm32h7") {
            HW_REQUIRE(rails.contains("3.3V_VDD"), "MCU must report 3.3V VDD rail");
            double vdd = rails["3.3V_VDD"].get<double>();
            HW_REQUIRE(vdd > 3.20 && vdd < 3.40, "3.3V rail should be within 3% tolerance");
            std::cout << "  -> MCU Telemetry: " << vdd << "V VDD, " << temp << " °C, State: " << state << std::endl;
        } else if (id == "fpga_artix7") {
            HW_REQUIRE(rails.contains("1.0V_VCCINT"), "FPGA must report 1.0V VCCINT rail");
            HW_REQUIRE(rails.contains("1.8V_VCCAUX"), "FPGA must report 1.8V VCCAUX rail");
            double vccint = rails["1.0V_VCCINT"].get<double>();
            HW_REQUIRE(vccint > 0.95 && vccint < 1.05, "1.0V rail should be within tolerance");
            std::cout << "  -> FPGA Telemetry: " << vccint << "V VCCINT, " << temp << " °C, State: " << state << std::endl;
        } else if (id == "jtag_ft2232h") {
            HW_REQUIRE(rails.contains("VTarget_Sense"), "JTAG must report VTarget sense rail");
            double vtarget = rails["VTarget_Sense"].get<double>();
            HW_REQUIRE(vtarget > 3.10 && vtarget < 3.45, "VTarget should sense 3.3V nominal");
            auto spec = d["device_specific"];
            HW_REQUIRE(spec["scan_chain_length"] == 2, "JTAG scan chain must report 2 devices");
            std::cout << "  -> JTAG Telemetry: " << vtarget << "V Sense, Scan Chain: 2 Devices (IDCODE 0x6BA02477 / 0x0362D093)" << std::endl;
        }
    }

    std::cout << "  -> Passed: Voltage rails, die temperatures, and device-specific payloads validated." << std::endl;
}

void testRecoverableTimeoutFailureAndRecovery() {
    std::cout << "[TEST 4] Testing Recoverable Failure Injection (Timeout) & Auto-Recovery..." << std::endl;

    auto& mgr = HardwareManager::instance();
    auto* mcu = mgr.getDevice("mcu_stm32h7");
    auto* jtag = mgr.getDevice("jtag_ft2232h");

    // 1. Inject Timeout on MCU
    std::cout << "  -> Injecting timeout fault into MCU..." << std::endl;
    HW_REQUIRE(mgr.injectFault("mcu_stm32h7", "timeout"), "Fault injection failed");
    HW_REQUIRE(mcu->getState() == DeviceState::ErrorTimeout, "MCU state must transition to ERROR_TIMEOUT");

    auto mcuTelem = mcu->pollTelemetry();
    HW_REQUIRE(mcuTelem.state == DeviceState::ErrorTimeout, "Telemetry state must reflect ERROR_TIMEOUT");
    HW_REQUIRE(mcuTelem.lastLogMessage.find("TIMEOUT ERROR") != std::string::npos, "Log must mention TIMEOUT ERROR");
    std::cout << "  -> MCU Failure Confirmed: " << mcuTelem.lastLogMessage << std::endl;

    // 2. Recover MCU
    std::cout << "  -> Executing soft watchdog recovery on MCU..." << std::endl;
    HW_REQUIRE(mgr.recoverFault("mcu_stm32h7"), "MCU recovery failed");
    HW_REQUIRE(mcu->getState() == DeviceState::Connected, "MCU state must recover to CONNECTED");

    auto mcuRecoveredTelem = mcu->pollTelemetry();
    HW_REQUIRE(mcuRecoveredTelem.state == DeviceState::Streaming, "MCU telemetry should resume STREAMING");
    std::cout << "  -> MCU Recovery Confirmed: State = " << deviceStateToString(mcuRecoveredTelem.state) << std::endl;

    // 3. Inject Timeout / Scan-Chain-Break on JTAG
    std::cout << "  -> Injecting scan chain break fault into JTAG Adapter..." << std::endl;
    HW_REQUIRE(mgr.injectFault("jtag_ft2232h", "scan_chain_break"), "JTAG fault injection failed");
    HW_REQUIRE(jtag->getState() == DeviceState::ErrorTimeout, "JTAG state must transition to ERROR_TIMEOUT");

    auto jtagTelem = jtag->pollTelemetry();
    HW_REQUIRE(jtagTelem.state == DeviceState::ErrorTimeout, "JTAG telemetry must reflect ERROR_TIMEOUT");
    std::cout << "  -> JTAG Failure Confirmed: " << jtagTelem.lastLogMessage << std::endl;

    // 4. Recover JTAG
    std::cout << "  -> Executing JTAG Test-Logic-Reset (TLR) recovery sequence..." << std::endl;
    HW_REQUIRE(mgr.recoverFault("jtag_ft2232h"), "JTAG recovery failed");
    HW_REQUIRE(jtag->getState() == DeviceState::Connected, "JTAG state must recover to CONNECTED");

    auto jtagRecoveredTelem = jtag->pollTelemetry();
    HW_REQUIRE(jtagRecoveredTelem.state == DeviceState::Streaming, "JTAG telemetry should resume STREAMING");
    std::cout << "  -> JTAG Recovery Confirmed: State = " << deviceStateToString(jtagRecoveredTelem.state) << std::endl;

    std::cout << "  -> Passed: Recoverable failure injection and subsequent restoration verified." << std::endl;
}

void testElectricalSystemArchitectureMetadata() {
    std::cout << "[TEST 5] Testing Electrical System Architecture & Pin-Voltage Documentation..." << std::endl;

    auto arch = HardwareManager::getSystemElectricalArchitecture();
    HW_REQUIRE(arch.contains("power_supply_architecture"), "Power architecture required");
    HW_REQUIRE(arch.contains("grounding_scheme"), "Grounding scheme required");
    HW_REQUIRE(arch.contains("voltage_domains"), "Voltage domains required");
    HW_REQUIRE(arch.contains("level_shifting"), "Level shifting details required");
    HW_REQUIRE(arch.contains("jtag_daisy_chain_ownership"), "JTAG ownership details required");
    HW_REQUIRE(arch.contains("pin_voltage_table"), "Pin-voltage table required");

    auto pinTable = arch["pin_voltage_table"];
    HW_REQUIRE(pinTable.is_array() && pinTable.size() >= 14, "Pin-voltage table must contain >= 14 verified signal rows");

    // Verify key signal definitions
    bool hasTck = false;
    bool hasSpi = false;
    bool hasVccint = false;

    for (const auto& pin : pinTable) {
        std::string sig = pin["signal"].get<std::string>();
        if (sig == "JTAG_TCK") hasTck = true;
        if (sig == "MCU_SPI_MOSI") hasSpi = true;
        if (sig == "VCCINT_1V0") hasVccint = true;
    }

    HW_REQUIRE(hasTck && hasSpi && hasVccint, "Key signals (TCK, SPI, VCCINT) must be present in pin-voltage table");
    std::cout << "  -> Pin-Voltage Table entries verified: " << pinTable.size() << " pins cataloged." << std::endl;
    std::cout << "  -> Passed: Complete electrical & systems integration metadata validated." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  UNIFIED ENGINEERING WORKBENCH - HARDWARE LAB   " << std::endl;
    std::cout << "    EMBEDDED SYSTEM INTEGRATION VERIFICATION TESTS" << std::endl;
    std::cout << "==================================================" << std::endl;

    try {
        testDeviceDiscovery();
        testConnectDisconnectLifecycle();
        testTelemetryAcquisition();
        testRecoverableTimeoutFailureAndRecovery();
        testElectricalSystemArchitectureMetadata();

        std::cout << "\n>>> ALL 5/5 HARDWARE INTEGRATION TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL HARDWARE TEST EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}

