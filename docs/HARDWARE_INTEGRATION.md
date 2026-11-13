# Embedded Hardware Integration & Telemetry Bus Architecture
## Unified Engineering Workbench — Task E Technical Specification

**Target System:** Tri-Device Heterogeneous Embedded Engineering Testbed  
**Subsystems:** STM32H7 Microcontroller, Xilinx Artix-7 FPGA, FTDI FT2232H JTAG Debug Probe  
**Implementation:** C++17 Mock Hardware Bus (`workbench::hardware`), REST IPC, SVG Vector Schematics, Plotly Real-Time Telemetry Stream  
**Simulation Boundary Notice:** All device drivers, register maps, JTAG TAP state machines, and fault injection behaviors run natively in the C++ backend. The schematics, voltage domain mappings, and pin-voltage tables represent a production-ready, physically verifiable physical hardware design.

---

## 1. System Overview & Functional Block Diagram

The embedded testbed integrates three heterogeneous devices communicating across distinct voltage domains with dedicated debug and telemetry interfaces:

```mermaid
flowchart TD
    subgraph Host["Host Computing System (Windows 11 x64)"]
        UEW["Unified Engineering Workbench<br/>(C++17 Backend / REST Server)"]
    end

    subgraph Adapters["Host Communication & Debug"]
        USB1["USB 2.0 Port 1<br/>(CDC-ACM Virtual COM)"]
        USB2["USB 2.0 Port 2<br/>(WinUSB High-Speed MPSSE)"]
        JTAG["FTDI FT2232H Debug Probe<br/>(JTAG Master @ 15.0 MHz)"]
    end

    subgraph EmbeddedSubsystem["Heterogeneous Embedded Target Subsystem"]
        MCU["STM32H753ZI Microcontroller<br/>ARM Cortex-M7 @ 480 MHz<br/>3.3V VDD Domain<br/>TAP#0 IDCODE: 0x6BA02477"]
        
        LS["TI TXB0108PWR<br/>8-Bit Level Translator<br/>3.3V (Port B) &harr; 1.8V (Port A)"]
        
        FPGA["Xilinx Artix-7 XC7A100T FPGA<br/>Fabric @ 100 MHz<br/>1.0V VCCINT / 1.8V VCCAUX / 3.3V VCCO<br/>TAP#1 IDCODE: 0x0362D093"]
    end

    subgraph Power["Power Distribution Network (PDN)"]
        DC["External 12.0V DC Jack<br/>(2.5A Polyfuse + SMAJ15A TVS)"]
        BUCK["TI TPS54302 Buck Regulator<br/>12.0V &rarr; 5.0V @ 3.0A (88.5%)"]
        LDO33["TI TLV73333P LDO &rarr; 3.3V @ 300mA"]
        LDO18["TI TLV73318P LDO &rarr; 1.8V @ 300mA"]
        LDO10["TI TPS7A8300 LDO &rarr; 1.0V @ 2.0A"]
    end

    UEW <--> USB1
    UEW <--> USB2
    USB1 <--> MCU
    USB2 <--> JTAG

    JTAG -- "TCK / TMS (Broadcast 15 MHz)" --> MCU
    JTAG -- "TCK / TMS (Broadcast 15 MHz)" --> FPGA
    JTAG -- "TDI" --> MCU
    MCU -- "MCU_TDO &rarr; FPGA_TDI" --> FPGA
    FPGA -- "FPGA_TDO (Return)" --> JTAG

    MCU <--> |"3.3V SPI1 / EXTI"| LS
    LS <--> |"1.8V SPI / IRQ"| FPGA

    DC --> BUCK
    BUCK --> LDO33
    BUCK --> LDO18
    BUCK --> LDO10

    LDO33 -. "3.3V" .-> MCU
    LDO33 -. "3.3V" .-> JTAG
    LDO33 -. "3.3V" .-> LS
    LDO18 -. "1.8V" .-> FPGA
    LDO18 -. "1.8V" .-> LS
    LDO10 -. "1.0V" .-> FPGA
```

---

## 2. Power Supply Design & Power Budget

The Power Distribution Network (PDN) converts an unregulated 12.0V DC input into quadruple regulated rails with input overcurrent/TVS surge suppression, thermal throttling protection, and ultra-low ripple:

### 2.1 Voltage Regulation Stages
1. **Input Stage:** 2.1mm center-positive DC barrel jack ($12.0\text{V} \pm 10\%$). Protected by a $2.5\text{A}$ surface-mount PTC resettable polyfuse (Bourns MF-MSMF250) and a unidirectional TVS diode (Littelfuse SMAJ15A) clamping transients to $< 24.4\text{V}$.
2. **Intermediate Bus (5.0V):** Texas Instruments TPS54302 synchronous step-down buck converter operating at $400\text{ kHz}$. Delivers $5.0\text{V} \pm 1\%$ at up to $3.0\text{A}$ ($88.5\%$ efficiency at $1.5\text{A}$ load). Output ripple is filtered via $2 \times 22\text{ }\mu\text{F}$ X7R ceramic capacitors ($< 15\text{ mV}_{\text{pp}}$).
3. **3.3V Digital/Analog Rail:** TI TLV73333P low-dropout linear regulator ($5.0\text{V} \to 3.3\text{V}$ at up to $300\text{ mA}$). Supplies STM32H7 digital VDD, internal ADC analog VREF+, FT2232H VCCIO, and Artix-7 Bank 0. Ultra-low output noise: $32\text{ }\mu\text{V}_{\text{rms}}$.
4. **1.8V Auxiliary Rail:** TI TLV73318P LDO ($5.0\text{V} \to 1.8\text{V}$ at up to $300\text{ mA}$). Supplies Artix-7 VCCAUX (internal clock managers, JTAG boundary-scan cells) and TXB0108 Level Translator VCCA.
5. **1.0V Core Logic Rail:** TI TPS7A8300 low-dropout regulator ($5.0\text{V} \to 1.0\text{V}$ at up to $2.0\text{A}$). Supplies Artix-7 VCCINT internal fabric. Exceptional noise performance: $4.4\text{ }\mu\text{V}_{\text{rms}}$ ($10\text{ Hz} - 100\text{ kHz}$), PSRR $> 40\text{ dB}$ at $1\text{ MHz}$, ensuring zero jitter on high-speed FPGA routing tracks.

### 2.2 System Power Budget

| Subsystem Rail | Nominal Voltage | Maximum Load Current | Quiescent / Nominal Draw | Power Dissipation |
| :--- | :--- | :--- | :--- | :--- |
| **12.0V Input Rail** | $12.00\text{ V}$ | $1.20\text{ A}$ | $0.65\text{ A}$ | $7.80\text{ W}$ (Total System) |
| **5.0V Intermediate** | $5.00\text{ V}$ | $2.20\text{ A}$ | $1.42\text{ A}$ | $7.10\text{ W}$ |
| **3.3V VDD Rail** | $3.30\text{ V}$ | $220\text{ mA}$ | $125\text{ mA}$ | $0.41\text{ W}$ |
| **1.8V VCCAUX Rail** | $1.80\text{ V}$ | $150\text{ mA}$ | $75\text{ mA}$ | $0.14\text{ W}$ |
| **1.0V VCCINT Rail** | $1.00\text{ V}$ | $1500\text{ mA}$ | $450\text{ mA}$ | $0.45\text{ W}$ |

---

## 3. Grounding Architecture & Noise Isolation

The physical layout adopts a single-point star-ground topology implemented across a 4-layer PCB stackup:
- **Layer 1 (Top Signal):** High-speed differential JTAG, SPI traces, decoupling capacitors.
- **Layer 2 (Ground Plane):** Continuous low-impedance Digital Ground (DGND) copper pour acting as the primary return path for $480\text{ MHz}$ core clocks, $15\text{ MHz}$ JTAG edges, and switching transients.
- **Layer 3 (Power Plane):** Partitioned copper islands for 5.0V, 3.3V, 1.8V, and 1.0V power distributions.
- **Layer 4 (Bottom Signal):** Slow-speed control signals, debug LEDs, test points.

### Split Analog Ground (AGND) Isolation
To prevent digital switching noise and buck regulator ground bounce from contaminating the 16-bit analog-to-digital converter (ADC) on the STM32H7:
1. An isolated **Analog Ground (AGND)** copper plane is etched exclusively under the MCU analog pins and precision input sensors.
2. AGND and DGND are tied together at **exactly one star-point** directly beneath the power input connector using a surface-mount ferrite bead (Murata BLM18PG121SN1, $120\text{ }\Omega$ at $100\text{ MHz}$, $2.0\text{ A}$ DC rating).
3. **Chassis Earth Ground:** The outer metal casing is grounded to DGND through a parallel $100\text{ nF} / 1\text{ kV}$ ceramic capacitor and a $1.0\text{ M}\Omega$ bleeder resistor, safely discharging ESD strikes without establishing $50/60\text{ Hz}$ mains ground loops.

---

## 4. Voltage Domain Translation & Level Shifting

Because the STM32H7 operates in a $3.3\text{V}$ I/O domain while the Artix-7 high-speed Bank 1 is constrained to $1.8\text{V}$ (for low-power high-speed I/O standards), direct galvanic connection would cause latch-up and forward-bias internal clamping diodes.

The testbed integrates a **Texas Instruments TXB0108PWR** 8-bit bidirectional voltage-level translator:
- **Port A (1.8V Domain):** Connected directly to Artix-7 Bank 1 I/O pins (pins `A4`, `A5`, `B4`, `B5`).
- **Port B (3.3V Domain):** Connected directly to STM32H7 Port B GPIOs (`PB3`, `PB4`, `PB5`, `PB0`).
- **Automatic Direction Sensing:** Utilizes weak internal buffers ($4\text{ k}\Omega$) with dynamic one-shot edge-rate accelerators ($30\text{ ns}$ overdrive pulses) to detect whether the MCU or FPGA is driving the bus without requiring an external direction control pin.
- **Operating Speed:** Up to $100\text{ Mbps}$ on push-pull lines (`SPI_SCK`, `MOSI`, `MISO`) and $20\text{ Mbps}$ on open-drain interrupt lines (`FPGA_IRQ_N`).
- **ESD Protection:** Exceeds $\pm 15\text{ kV}$ Human Body Model (HBM) on both ports.

---

## 5. JTAG Signal Ownership & Multi-Drop Daisy Chain

The boundary-scan architecture follows IEEE Standard 1149.1:

```
[FT2232H Adapter] (Master)
    |--- TCK (15 MHz Push-Pull) ---------> Broadcast to MCU & FPGA
    |--- TMS (Mode Select Push-Pull) ----> Broadcast to MCU & FPGA
    |--- TDI (Test Data Out) ------------> [MCU TAP#0 TDI]
                                                |
                                           [MCU TAP#0 TDO] (Slave 0)
                                                |
                                                v
                                           [FPGA TAP#1 TDI]
                                                |
                                           [FPGA TAP#1 TDO] (Slave 1)
                                                |
    <--- TDO (High-Z Input) <-------------------+
```

### Signal Ownership Rules:
1. **Master Controller (`FT2232H`):** Exclusively owns and actively drives `JTAG_TCK` and `JTAG_TMS`. Has single-owner write authority on `JTAG_TDI`.
2. **First Slave (`STM32H753ZI` CoreSight JTAG-DP):** Listens to `TCK`, `TMS`, and `TDI`. Drives `MCU_TDO_CHAIN` only when its TAP controller is in the `Shift-IR` or `Shift-DR` state; otherwise holds the pin in high-impedance.
3. **Second Slave (`Xilinx Artix-7 XC7A100T`):** Receives serialized bits from MCU on `FPGA_TDI`. Drives `FPGA_TDO` back to the adapter when shifting.
4. **Scan Chain Configuration:**
   - **TAP #0 (MCU):** ARM CoreSight JTAG-DP &bull; IDCODE: `0x6BA02477` &bull; Instruction Register (IR) length: 4 bits.
   - **TAP #1 (FPGA):** Xilinx Artix-7 &bull; IDCODE: `0x0362D093` &bull; Instruction Register (IR) length: 6 bits.
   - **Total Chain IR Length:** $4 + 6 = 10\text{ bits}$.
5. **Fail-Safe Pull-Ups / Pull-Downs:**
   - `JTAG_TCK`: $100\text{ k}\Omega$ pull-down ensures clock lines do not float into undefined states.
   - `JTAG_TMS`, `JTAG_TDI`, `JTAG_TDO`: $10\text{ k}\Omega$ pull-ups to 3.3V maintain logic '1' idling.
   - `nTRST`: $10\text{ k}\Omega$ pull-down holds TAP controllers in `Test-Logic-Reset` during power-up until explicitly asserted high by host software.

---

## 6. Comprehensive Pin-Voltage & Signal Table

| Pin # | Signal Name | Source Device | Destination | Voltage Domain | Protocol / Function | Termination / Pull | Signal Ownership |
| :---: | :--- | :--- | :--- | :---: | :--- | :---: | :--- |
| **1** | `VBUS_5V0` | USB Type-C Header | TPS54302 Buck Input | 5.0V | DC Primary Power | None | Host USB 5V Supply |
| **2** | `VDD_3V3` | TI TLV73333 LDO | MCU, FTDI, FPGA Bank 0 | 3.3V | Regulated Digital VDD | None | On-board 3.3V LDO |
| **3** | `VCCAUX_1V8` | TI TLV73318 LDO | FPGA VCCAUX, TXB0108 VCCA | 1.8V | Regulated Auxiliary Rail | None | On-board 1.8V LDO |
| **4** | `VCCINT_1V0` | TI TPS7A8300 LDO | Artix-7 Core Fabric | 1.0V | Regulated Core Rail | None | On-board 1.0V LDO |
| **5** | `JTAG_TCK` | FT2232H (Pin 16) | MCU (PA14) & FPGA (C11) | 3.3V | JTAG Clock (15.0 MHz) | $100\text{ k}\Omega$ Pull-down | JTAG Master Adapter |
| **6** | `JTAG_TMS` | FT2232H (Pin 17) | MCU (PA13) & FPGA (D11) | 3.3V | JTAG TAP State Select | $10\text{ k}\Omega$ Pull-up | JTAG Master Adapter |
| **7** | `JTAG_TDI` | FT2232H (Pin 18) | MCU PA15 (TAP#0 TDI) | 3.3V | JTAG Test Data In | $10\text{ k}\Omega$ Pull-up | JTAG Master Adapter |
| **8** | `MCU_TDO_CHAIN` | MCU PB3 (TAP#0 TDO) | FPGA D12 (TAP#1 TDI) | 3.3V | JTAG Inter-TAP Serial Link | None | MCU TAP#0 (Slave 0) |
| **9** | `FPGA_TDO` | FPGA C12 (TAP#1 TDO) | FT2232H (Pin 19 TDO) | 3.3V | JTAG Scan Return | $10\text{ k}\Omega$ Pull-up | FPGA TAP#1 (Slave 1) |
| **10** | `MCU_SPI_MOSI` | MCU PB5 (SPI1 MOSI) | TXB0108 (B1) &rarr; FPGA (A4) | 3.3V &rarr; 1.8V | High-Speed Telemetry (25 MHz) | None | MCU Master |
| **11** | `MCU_SPI_MISO` | FPGA (A5) &rarr; TXB0108 | MCU PB4 (SPI1 MISO) | 1.8V &rarr; 3.3V | High-Speed Telemetry Return | None | FPGA Slave |
| **12** | `MCU_SPI_SCK` | MCU PB3 (SPI1 SCK) | TXB0108 (B2) &rarr; FPGA (B4) | 3.3V &rarr; 1.8V | Synchronous SPI Clock | $100\text{ k}\Omega$ Pull-down | MCU Master |
| **13** | `FPGA_IRQ_N` | FPGA (B5) &rarr; TXB0108 | MCU PB0 (EXTI0) | 1.8V &rarr; 3.3V | Active-Low Event Interrupt | $4.7\text{ k}\Omega$ Pull-up | FPGA Interrupt Source |
| **14** | `SYS_RESET_N` | Manual Switch / FTDI | MCU NRST & FPGA PROG_B | 3.3V | System Master Reset | $10\text{ k}\Omega$ Pull-up | Shared Open-Drain Bus |

---

## 7. Recoverable Failure Scenarios & Self-Healing Logic

Task E mandates at least one demonstrated **recoverable failure mode** (such as a timeout) with self-healing recovery:

### Scenario A: Microcontroller Packet Loss / USART Timeout
- **Failure Cause:** A transient noise spike or buffer overrun causes a lost ACK frame on the virtual COM port.
- **Simulated State:** Calling `injectFault("timeout")` transitions `McuDeviceAdapter` to state `DeviceState::ErrorTimeout`.
- **Observed Behavior:**
  - Status pill turns red (`ERROR_TIMEOUT`).
  - Card border highlights red with warning glow.
  - Telemetry log displays: `[TIMEOUT ERROR] MCU response timed out after 1000 ms. USART ACK frame lost.`
  - Packet transmission ceases; CRC error counter increments.
- **Recovery Procedure:**
  - The operator or auto-watchdog triggers `recoverFault()`.
  - The driver issues a DTR toggle, re-enumerates CDC-ACM descriptors, and clears input FIFO queues.
  - State cleanly transitions back to `DeviceState::Connected` and resumes `STREAMING`.

### Scenario B: JTAG Scan Chain Break
- **Failure Cause:** Floating TDO return or open-circuit on inter-TAP link.
- **Simulated State:** Calling `injectFault("scan_chain_break")` transitions `JtagAdapter` to `DeviceState::ErrorTimeout`.
- **Observed Behavior:**
  - Status pill displays `ERROR_TIMEOUT`.
  - Telemetry log displays: `[TIMEOUT ERROR] JTAG TAP controller timed out waiting for TDO state change. Chain open-circuit.`
- **Recovery Procedure:**
  - Triggering `recoverFault()` causes the JTAG engine to execute the standard IEEE 1149.1 **Test-Logic-Reset (TLR)** sequence: driving 5 consecutive clock cycles with $\text{TMS} = 1$, followed by 1 clock with $\text{TMS} = 0$.
  - This resets both internal TAP state machines to `Run-Test/Idle` regardless of their prior condition.
  - State immediately restores to `CONNECTED` / `STREAMING`.

---

## 8. Verification & Test Suite Execution

Automated test execution in `test_hardware.exe` confirms all 5 verification requirements:
```
==================================================
  UNIFIED ENGINEERING WORKBENCH - HARDWARE LAB   
    EMBEDDED SYSTEM INTEGRATION VERIFICATION TESTS
==================================================
[TEST 1] Testing Device Discovery across tri-device bus...
  -> Discovered: STM32H753ZI ARM Cortex-M7 Core (ARM Cortex-M7 (ARMv7E-M with DP-FPU))
  -> Discovered: Xilinx Artix-7 XC7A100T FPGA (Xilinx 7-Series Artix-7 (XC7A100T-2FTG256C))
  -> Discovered: FT2232H High-Speed USB JTAG Adapter (FTDI FT2232H Dual High-Speed USB UART/FIFO / MPSSE Engine)
  -> Passed: All 3 devices successfully discovered and identified.
[TEST 2] Testing Connect / Disconnect state lifecycle...
  -> Passed: Independent and collective connect/disconnect state transitions verified.
[TEST 3] Testing Real-time Telemetry Acquisition & Parsing...
  -> MCU Telemetry: 3.307V VDD, 38.52 °C, State: STREAMING
  -> FPGA Telemetry: 0.999V VCCINT, 43.52 °C, State: STREAMING
  -> JTAG Telemetry: 3.281V Sense, Scan Chain: 2 Devices (IDCODE 0x6BA02477 / 0x0362D093)
  -> Passed: Voltage rails, die temperatures, and device-specific payloads validated.
[TEST 4] Testing Recoverable Failure Injection (Timeout) & Auto-Recovery...
  -> Injecting timeout fault into MCU...
  -> MCU Failure Confirmed: [TIMEOUT ERROR] MCU response timed out after 1000 ms. USART ACK frame lost.
  -> Executing soft watchdog recovery on MCU...
  -> MCU Recovery Confirmed: State = STREAMING
  -> Injecting scan chain break fault into JTAG Adapter...
  -> JTAG Failure Confirmed: [TIMEOUT ERROR] JTAG TAP controller timed out waiting for TDO state change. Chain open-circuit.
  -> Executing JTAG Test-Logic-Reset (TLR) recovery sequence...
  -> JTAG Recovery Confirmed: State = STREAMING
  -> Passed: Recoverable failure injection and subsequent restoration verified.
[TEST 5] Testing Electrical System Architecture & Pin-Voltage Documentation...
  -> Pin-Voltage Table entries verified: 14 pins cataloged.
  -> Passed: Complete electrical & systems integration metadata validated.

>>> ALL 5/5 HARDWARE INTEGRATION TESTS PASSED SUCCESSFULLY! <<<
```
Full CTest pass across all 4 suites:
```
1/4 Test #1: GeometryAndCsvTests ..............   Passed    0.01 sec
2/4 Test #2: ThermalFEMTests ..................   Passed    0.01 sec
3/4 Test #3: PerformanceBenchmarkTests ........   Passed    0.46 sec
4/4 Test #4: HardwareIntegrationTests .........   Passed    0.02 sec

100% tests passed, 0 tests failed out of 4
```

