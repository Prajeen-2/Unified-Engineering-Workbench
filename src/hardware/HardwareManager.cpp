#include "hardware/HardwareManager.hpp"

namespace workbench::hardware {

HardwareManager& HardwareManager::instance() {
    static HardwareManager s_instance;
    return s_instance;
}

HardwareManager::HardwareManager() {
    m_devices["mcu_stm32h7"] = std::make_unique<McuDeviceAdapter>();
    m_devices["fpga_artix7"] = std::make_unique<FpgaDeviceAdapter>();
    m_devices["jtag_ft2232h"] = std::make_unique<JtagAdapter>();

    // Start with devices in Connected/Streaming state for initial inspection
    for (auto& pair : m_devices) {
        pair.second->connect(500);
    }
}

std::vector<DeviceCapability> HardwareManager::getDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    std::vector<DeviceCapability> caps;
    caps.reserve(m_devices.size());

    // Explicit ordering: MCU, FPGA, JTAG
    const std::vector<std::string> order = {"mcu_stm32h7", "fpga_artix7", "jtag_ft2232h"};
    for (const auto& id : order) {
        auto it = m_devices.find(id);
        if (it != m_devices.end()) {
            caps.push_back(it->second->getCapabilities());
        }
    }
    return caps;
}

IHardwareDevice* HardwareManager::getDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    auto it = m_devices.find(deviceId);
    if (it != m_devices.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool HardwareManager::connectDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    auto it = m_devices.find(deviceId);
    if (it != m_devices.end()) {
        return it->second->connect();
    }
    return false;
}

bool HardwareManager::disconnectDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    auto it = m_devices.find(deviceId);
    if (it != m_devices.end()) {
        return it->second->disconnect();
    }
    return false;
}

bool HardwareManager::connectAll() {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    bool ok = true;
    for (auto& pair : m_devices) {
        ok = pair.second->connect() && ok;
    }
    return ok;
}

bool HardwareManager::disconnectAll() {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    bool ok = true;
    for (auto& pair : m_devices) {
        ok = pair.second->disconnect() && ok;
    }
    return ok;
}

nlohmann::json HardwareManager::getAllTelemetry() {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    nlohmann::json jDevices = nlohmann::json::array();

    const std::vector<std::string> order = {"mcu_stm32h7", "fpga_artix7", "jtag_ft2232h"};
    for (const auto& id : order) {
        auto it = m_devices.find(id);
        if (it != m_devices.end()) {
            jDevices.push_back(it->second->pollTelemetry().toJson());
        }
    }

    return nlohmann::json{
        {"success", true},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"device_count", m_devices.size()},
        {"is_simulated_backend", true},
        {"devices", jDevices}
    };
}

bool HardwareManager::injectFault(const std::string& deviceId, const std::string& faultType) {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    auto it = m_devices.find(deviceId);
    if (it != m_devices.end()) {
        return it->second->injectFault(faultType);
    }
    return false;
}

bool HardwareManager::recoverFault(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    auto it = m_devices.find(deviceId);
    if (it != m_devices.end()) {
        return it->second->recoverFault();
    }
    return false;
}

bool HardwareManager::writeRegister(const std::string& deviceId, uint32_t address, uint32_t value) {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    auto it = m_devices.find(deviceId);
    if (it != m_devices.end()) {
        return it->second->writeRegister(address, value);
    }
    return false;
}

bool HardwareManager::readRegister(const std::string& deviceId, uint32_t address, uint32_t& outValue) {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    auto it = m_devices.find(deviceId);
    if (it != m_devices.end()) {
        return it->second->readRegister(address, outValue);
    }
    return false;
}

bool HardwareManager::verifyJtagLoopback(const std::vector<uint8_t>& pattern, std::vector<uint8_t>& outEcho) {
    std::lock_guard<std::mutex> lock(m_managerMutex);
    auto it = m_devices.find("jtag_ft2232h");
    if (it != m_devices.end()) {
        auto* jtag = dynamic_cast<JtagAdapter*>(it->second.get());
        if (jtag) {
            return jtag->verifyLoopback(pattern, outEcho);
        }
    }
    return false;
}

nlohmann::json HardwareManager::getSystemElectricalArchitecture() {
    return nlohmann::json{
        {"system_title", "Tri-Device Heterogeneous Embedded System Integration"},
        {"simulation_notice", "Simulated C++ Mock Bus Engine. Schematics & pin tables represent production physical circuit design."},
        {"power_supply_architecture", {
            {"input_power", "12.0V DC (+/- 10%) Barrel Jack (2.1mm Center-Positive, 2.5A Polyfuse, SMAJ15A TVS Diode)"},
            {"switching_regulator", "TI TPS54302 Synchronous Step-Down Buck Regulator (12V -> 5.0V @ 3.0A, 88.5% Efficiency)"},
            {"ldo_3v3", "TI TLV73333P Low-Dropout Linear Regulator (5.0V -> 3.3V @ 300 mA, 32 uVrms Noise) [MCU VDD, FPGA VCCO_0, JTAG VIO]"},
            {"ldo_1v8", "TI TLV73318P Low-Dropout Linear Regulator (5.0V -> 1.8V @ 300 mA) [FPGA VCCAUX, Level Shifter VCCA]"},
            {"ldo_1v0", "TI TPS7A8300 Ultra-Low-Noise LDO (5.0V -> 1.0V @ 2.0A, 4.4 uVrms Noise) [FPGA VCCINT Core Fabric]"}
        }},
        {"grounding_scheme", {
            {"topology", "Single-Point Star Grounding Scheme with Split Digital & Analog Planes"},
            {"digital_ground", "DGND Plane (4-layer stackup Layer 2 continuous copper pour, return path for 480 MHz & 15 MHz JTAG)"},
            {"analog_ground", "AGND Island isolated for MCU Precision ADC, joined to DGND at power input via BLM18PG121SN1 Ferrite Bead"},
            {"chassis_earth", "Chassis Ground bonded to DGND via parallel 100 nF 1 kV Ceramic Capacitor + 1.0 MOhm bleeder resistor"}
        }},
        {"voltage_domains", {
            {"domain_5v0", "5.0V Main Bus Rail (Buck Regulator Output / USB VBUS Input)"},
            {"domain_3v3", "3.3V Digital I/O Domain (STM32H7 VDD, FT2232H VCCIO, Artix-7 Bank 0)"},
            {"domain_1v8", "1.8V Auxiliary & High-Speed I/O Domain (Artix-7 VCCAUX, Bank 1)"},
            {"domain_1v0", "1.0V Core Logic Domain (Artix-7 FPGA VCCINT Internal Fabric)"}
        }},
        {"level_shifting", {
            {"part_number", "Texas Instruments SN74AXC8T245 8-Bit Dual-Supply Bus Transceiver"},
            {"port_a_voltage", "1.8V (Connected to FPGA Bank 1)"},
            {"port_b_voltage", "3.3V (Connected to MCU Port B SPI & Control Signals)"},
            {"bandwidth", "Up to 380 Mbps (Push-Pull CMOS Drive)"},
            {"features", "Direction-Controlled Push-Pull CMOS Buffers, High Drive (12 mA), Eliminates Voltage-Divider Contention with 10k/4.7k Pull-ups"}
        }},
        {"jtag_daisy_chain_ownership", {
            {"master_controller", "FTDI FT2232H MPSSE Engine (Exclusive push-pull owner of TCK, TMS, TDI)"},
            {"tap_0_mcu", "STM32H753ZI ARM CoreSight JTAG-DP (IDCODE: 0x6BA02477, IR Length: 4 bits)"},
            {"tap_1_fpga", "Xilinx Artix-7 XC7A100T (IDCODE: 0x0362D093, IR Length: 6 bits)"},
            {"daisy_chain_path", "FT2232H TDI -> MCU TDI -> MCU TDO -> FPGA TDI -> FPGA TDO -> FT2232H TDO (High-Z Input)"},
            {"reset_signal", "nTRST actively driven low during power-on reset; 10 kOhm pull-down ensures safe TAP hold in Test-Logic-Reset"}
        }},
        {"pin_voltage_table", {
            {{"pin", 1}, {"signal", "VBUS_5V0"}, {"source", "USB Connector"}, {"destination", "TPS54302 Input"}, {"voltage", "5.0V"}, {"protocol", "Power"}, {"pull", "None"}, {"ownership", "Host USB Supply"}},
            {{"pin", 2}, {"signal", "VDD_3V3"}, {"source", "TLV73333 LDO"}, {"destination", "MCU, FTDI, FPGA B0"}, {"voltage", "3.3V"}, {"protocol", "Power"}, {"pull", "None"}, {"ownership", "On-board LDO"}},
            {{"pin", 3}, {"signal", "VCCAUX_1V8"}, {"source", "TLV73318 LDO"}, {"destination", "FPGA VCCAUX, Level Shifter VCCA"}, {"voltage", "1.8V"}, {"protocol", "Power"}, {"pull", "None"}, {"ownership", "On-board LDO"}},
            {{"pin", 4}, {"signal", "VCCINT_1V0"}, {"source", "TPS7A8300 LDO"}, {"destination", "FPGA Core VCCINT"}, {"voltage", "1.0V"}, {"protocol", "Power"}, {"pull", "None"}, {"ownership", "On-board LDO"}},
            {{"pin", 5}, {"signal", "JTAG_TCK"}, {"source", "FT2232H (Pin 16)"}, {"destination", "MCU (PA14) & FPGA (C11)"}, {"voltage", "3.3V"}, {"protocol", "JTAG Clock (15 MHz)"}, {"pull", "100k Pull-down"}, {"ownership", "JTAG Adapter (Master)"}},
            {{"pin", 6}, {"signal", "JTAG_TMS"}, {"source", "FT2232H (Pin 17)"}, {"destination", "MCU (PA13) & FPGA (D11)"}, {"voltage", "3.3V"}, {"protocol", "JTAG Mode Select"}, {"pull", "10k Pull-up"}, {"ownership", "JTAG Adapter (Master)"}},
            {{"pin", 7}, {"signal", "JTAG_TDI"}, {"source", "FT2232H (Pin 18)"}, {"destination", "MCU (PA15, TAP#0)"}, {"voltage", "3.3V"}, {"protocol", "JTAG Data In"}, {"pull", "10k Pull-up"}, {"ownership", "JTAG Adapter (Master)"}},
            {{"pin", 8}, {"signal", "MCU_TDO_CHAIN"}, {"source", "MCU (PB3, TAP#0)"}, {"destination", "FPGA (D12, TAP#1 TDI)"}, {"voltage", "3.3V"}, {"protocol", "JTAG Inter-device Chain"}, {"pull", "None"}, {"ownership", "MCU TAP (Slave 0)"}},
            {{"pin", 9}, {"signal", "FPGA_TDO"}, {"source", "FPGA (C12, TAP#1)"}, {"destination", "FT2232H (Pin 19 TDO)"}, {"voltage", "3.3V"}, {"protocol", "JTAG Data Return"}, {"pull", "10k Pull-up"}, {"ownership", "FPGA TAP (Slave 1)"}},
            {{"pin", 10}, {"signal", "MCU_SPI_MOSI"}, {"source", "MCU (PB5, SPI1)"}, {"destination", "SN74AXC8T245 (B1) -> FPGA (A4)"}, {"voltage", "3.3V -> 1.8V"}, {"protocol", "SPI Telemetry Bus (25 MHz)"}, {"pull", "None"}, {"ownership", "MCU (Master)"}},
            {{"pin", 11}, {"signal", "MCU_SPI_MISO"}, {"source", "FPGA (A5) -> SN74AXC8T245"}, {"destination", "MCU (PB4, SPI1)"}, {"voltage", "1.8V -> 3.3V"}, {"protocol", "SPI Telemetry Bus"}, {"pull", "None"}, {"ownership", "FPGA (Slave)"}},
            {{"pin", 12}, {"signal", "MCU_SPI_SCK"}, {"source", "MCU (PB3, SPI1)"}, {"destination", "SN74AXC8T245 (B2) -> FPGA (B4)"}, {"voltage", "3.3V -> 1.8V"}, {"protocol", "SPI Clock"}, {"pull", "100k Pull-down"}, {"ownership", "MCU (Master)"}},
            {{"pin", 13}, {"signal", "FPGA_IRQ_N"}, {"source", "FPGA (B5) -> SN74AXC8T245"}, {"destination", "MCU (PB0, EXTI0)"}, {"voltage", "1.8V -> 3.3V"}, {"protocol", "Active-Low Interrupt"}, {"pull", "4.7k Pull-up"}, {"ownership", "FPGA (Interrupt Source)"}},
            {{"pin", 14}, {"signal", "SYS_RESET_N"}, {"source", "Pushbutton / FTDI"}, {"destination", "MCU NRST & FPGA PROG_B"}, {"voltage", "3.3V"}, {"protocol", "System Hardware Reset"}, {"pull", "10k Pull-up"}, {"ownership", "Shared Open-Drain Bus"}}
        }}
    };
}

} // namespace workbench::hardware

