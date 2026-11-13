# UNIFIED ENGINEERING WORKBENCH
## PASS SCIENCE WORKS R&D Internship 2026–2027 Selection Project
*Candidate Submission Package | Version: 1.0.0-Release Candidate | Target: Windows 11 x64*

---

### Executive Overview

The **Unified Engineering Workbench** is an integrated, high-performance desktop engineering simulation, CAE visualization, multicore benchmarking, embedded system telemetry, and manual neural computing platform.

All numerical solvers, file parsers, concurrency thread pools, hardware state machines, and REST communication endpoints are implemented strictly in **pure ISO C++17** with **zero Python and zero external runtime dependencies**. The user interface is built on a responsive dark engineering frontend utilizing **Three.js r128** (WebGL hardware-accelerated 3D CAD and thermal cloud visualization) and **Plotly.js v2.32** (vector scientific graphing)—all hosted 100% locally from `frontend/vendor/` with **zero external CDN dependencies**.

---

### Core Technology Stack

| Layer | Component | Specification |
| :--- | :--- | :--- |
| **Backend Computation** | ISO C++17 (MSVC 19.51 / x64) | All file parsing, FEM solvers, thread pools, and neural calculus in C++ |
| **Concurrency Model** | Standard Library (`std::thread`) | Bounded multicore worker pool with mutexes and condition variables |
| **Build Management** | CMake 3.16+ / MSBuild | Native multi-configuration CMake project configured for Release (`/O2`) |
| **Frontend Framework** | HTML5 / CSS3 / ES6 | Premium Dark Engineering UI, responsive design, consistent layout |
| **3D CAD Visualization** | Three.js (r128) + OrbitControls | WebGL hardware-accelerated 3D facet rendering & point clouds |
| **Scientific Graphing** | Plotly.js (v2.32) | Interactive vector-grade scientific graphing and convergence plots |
| **JSON Serialization** | nlohmann/json (v3.11.3) | Header-only modern C++ JSON standard |
| **REST Server** | cpp-httplib (v0.18.3) | Embedded HTTP server running on Windows Sockets (`ws2_32`) |
| **Target Platform** | Windows 11 x64 | Native Win32 desktop application with `Launch.exe` root entrypoint |

---

### Zero-Friction Evaluator Launch

To evaluate this application with **zero terminal commands, zero PowerShell, zero administrator rights, zero package installs, and zero internet connectivity**:

1. Extract the submission ZIP package (`CandidateID_Workbench.zip`).
2. Double-click **`Launch.exe`** located in the root directory.
3. The Win32 GUI launcher initializes `workbench_server.exe` in the background (hidden, zero visible console windows), waits until `http://127.0.0.1:8080/api/health` responds, and automatically opens the user's default web browser:
   $$\text{http://127.0.0.1:8080/}$$
4. If the server is already running, `Launch.exe` safely brings up the browser without spawning redundant processes.

---

### VS Code Single-Action Launch

To build and run directly within Visual Studio Code in one action:
- **Prerequisite:** Install the **C/C++ extension** (`ms-vscode.cpptools` or C/C++ Extension Pack).
- Open the project folder in VS Code.
- Press **`F5`** (or select **Run -> Start Debugging**).
  - This automatically triggers the `Build All (Release)` task via `${workspaceFolder}\build.bat`.
  - Upon successful build, it launches `${workspaceFolder}\Launch.exe`, which starts the backend server and opens the browser.

---

### Source Build & Packaging Instructions

#### Option A: One-Click Automated Build (`build.bat`)
Run the build script from any command prompt:
```bat
build.bat
```
This script dynamically detects Visual Studio installations via `vswhere.exe` (or utilizes an active x64 Developer Command Prompt environment), invokes CMake in Release mode (`/O2`, static MSVC runtime `/MT`), compiles all binaries and tests, and deploys `Launch.exe` and `workbench_server.exe` to the root directory.

#### Option B: Submission Packaging Script (`package.bat`)
To generate the submission ZIP archive without PowerShell or third-party tools:
```bat
package.bat [CandidateID]
```
This script validates that all executables are built and uses the native Windows `tar.exe` to assemble `CandidateID_Workbench.zip` with clean POSIX forward-slash paths.

---

### Licensing & Third-Party Dependencies

All third-party libraries (cpp-httplib, nlohmann/json, Three.js, OrbitControls, Plotly.js) are documented with exact version pins, upstream repositories, and licenses in:
- [`THIRD_PARTY_NOTICES.md`](file:///d:/Internship%20app/THIRD_PARTY_NOTICES.md)
- [`DEPENDENCIES.lock.md`](file:///d:/Internship%20app/DEPENDENCIES.lock.md)

#### Option B: Manual CMake Build
```powershell
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

#### Running the Full Automated CTest Suite
To run all 5 automated C++ verification test suites:
```powershell
ctest --test-dir build -C Release --output-on-failure
```
**Observed Result:** 5/5 test suites passed (100% pass rate in ~0.50 seconds).

---

### Integrated Module Verification & Compliance Matrix

| Module | Task ID | Core Technical Specifications | Verification Tolerance | Observed Status |
| :--- | :---: | :--- | :--- | :---: |
| **Dashboard** | Task A | Host architecture telemetry, physical/logical CPU cores, RAM, compiler flags, launch latency | Live REST telemetry | **PASS** |
| **Geometry Viewer** | Task B | ASCII STL parser (20 facets, 60 coords), ANSYS CSV parser (108 nodes), Z-up Three.js CAD viewport | Solid name: `EngineeringBracket`, Dimensions: $100 \times 50 \times 80$ mm | **PASS** |
| **Thermal Analysis** | Task C | 1D steady-state heat equation FEM solver, linear 2-node elements, Dirichlet boundaries | Midpoint error $\le 125/N^2$, $\alpha \approx 2.0$ convergence rate ($N=10, 20, 40, 80$) | **PASS** |
| **Performance Lab** | Task D | Bounded `std::thread` pool (1–4 workers), 5 measured runs + warm-up, statistical median | Numerical invariance $\Delta T \le 1.0 \times 10^{-8}$ °C vs serial | **PASS** |
| **Hardware Integration** | Task E | Tri-device embedded integration (STM32H7 MCU, Artix-7 FPGA, FT2232H JTAG probe), timeout fault | Automatic recovery via Test-Logic-Reset (TLR), live voltage rail strip charts | **PASS** |
| **Neural Network Lab** | Task F | Pure C++ manual chain-rule backward pass (zero autograd), 2-sided central finite differences | Gradient discrepancy $\le 1.0 \times 10^{-5}$ ($\epsilon = 10^{-6}$), Peak error: $1.61 \times 10^{-10}$ | **PASS** |
| **Reports & Exports** | Task G | Centralized CSV download hub (8 production artifacts), live C++ log terminal, root `Launch.exe` | 100% self-contained offline delivery | **PASS** |

---

### Production Deliverables & Data Exports

All numerical datasets, convergence logs, and engineering reports can be downloaded directly from the **Reports & Exports** tab in the application or accessed in the repository:

1. **`thermal_temperature_results.csv`**: Spatial nodal coordinates, FEM numerical temperatures, analytical temperatures, and pointwise errors.
2. **`thermal_mesh_convergence.csv`**: Mesh convergence table across $N = 10, 20, 40, 80$ elements detailing element length $h$, maximum midpoint error, theoretical error bounds, and convergence order $\alpha$.
3. **`multicore_benchmark_results.csv`**: Statistical benchmark measurements across 1 to 4 worker threads, 5 measured runs, median time $T_p$, speedup $S_p$, parallel efficiency $E_p$, and $\Delta T$ verification.
4. **`hardware_pin_architecture.csv`**: 14-pin electrical table for STM32H7 MCU, Artix-7 FPGA, and FT2232H JTAG probe specifying pin numbers, signal names, voltage domains (5.0V, 3.3V, 1.8V, 1.0V), protocols, and bus ownership.
5. **`neural_backprop_training_log.csv`**: 100-step neural training matrix with parameter trajectories ($w, b, v, c$), pre-activation $z$, activation $h$, output $\hat{y}$, residual error $e$, loss $J$, analytical gradients, and finite-difference check discrepancies.
6. **`docs/MANUAL_BACKPROP_DERIVATION.md`**: First-principles mathematical derivation of the manual chain rule, symmetric Taylor series truncation error bounds proof, and Step 0 hand-calculated benchmark table formatted for a 2-page appendix.
7. **`docs/HARDWARE_INTEGRATION.md`**: Complete electrical systems engineering documentation covering power trees, level shifting (TI TXB0108), star grounding schemes, and IEEE 1149.1 JTAG daisy chain discipline.
8. **`docs/PERFORMANCE_LAB.md`**: Multicore thread-pool architecture, deterministic task queue ordering, statistical median timing analysis, and Amdahl's Law scaling limits.

---

### Repository Directory Structure

```
unified-engineering-workbench/
├── frontend/                     # Modern Dark Engineering UI
│   ├── css/
│   │   └── style.css             # High-contrast engineering stylesheet
│   ├── js/
│   │   ├── api.js                # REST API client & latency tracker
│   │   ├── app.js                # UI controller & telemetry poller
│   │   ├── geometry_viewer.js    # Three.js 3D CAD & thermal cloud visualizer
│   │   ├── thermal_analysis.js   # Thermal FEA controller & Plotly charts
│   │   ├── performance_lab.js    # Multicore benchmark controller & charts
│   │   ├── hardware_integration.js # Tri-device telemetry & fault injection
│   │   ├── neural_lab.js         # Neural manual backpropagation controller
│   │   └── reports_exports.js    # Reports hub, log streamer & file exporter
│   ├── vendor/                   # Bundled offline libraries (Zero CDN dependency)
│   │   ├── three.min.js          # Three.js r128 3D library
│   │   ├── OrbitControls.js      # CAD camera orbit controller
│   │   └── plotly.min.js         # Scientific graphing engine
│   └── index.html                # Unified Workbench Single-Page Application
│
├── src/                          # C++17 Implementation
│   ├── core/                     # System Telemetry & Thread-safe Logger
│   │   ├── Logger.cpp
│   │   └── SystemInfo.cpp
│   ├── server/                   # REST HTTP Server & Static Asset Dispatcher
│   │   └── HttpServer.cpp
│   ├── launcher/                 # Win32 Zero-Config Application Launcher
│   │   └── launch.cpp
│   ├── geometry/                 # Task B (STL & CSV parsers)
│   │   ├── StlParser.cpp
│   │   └── CsvParser.cpp
│   ├── thermal/                  # Task C (1D FEA solver)
│   │   └── ThermalSolver1D.cpp
│   ├── performance/              # Task D (Worker pool benchmark)
│   │   ├── ThreadPool.cpp
│   │   └── PerformanceBenchmark.cpp
│   ├── hardware/                 # Task E (MCU/FPGA/JTAG mock adapters)
│   │   ├── McuDeviceAdapter.cpp
│   │   ├── FpgaDeviceAdapter.cpp
│   │   ├── JtagAdapter.cpp
│   │   └── HardwareManager.cpp
│   ├── neural/                   # Task F (Manual backprop engine)
│   │   └── NeuralNetRegression.cpp
│   └── main.cpp                  # Application backend entrypoint
│
├── include/                      # C++ Header Interfaces
│   ├── core/
│   ├── server/
│   ├── geometry/
│   ├── thermal/
│   ├── performance/
│   ├── hardware/
│   └── neural/
│
├── third_party/                  # Header-only C++ Libraries
│   ├── nlohmann/json.hpp         # JSON standard library
│   └── httplib/httplib.h         # cpp-httplib networking engine
│
├── data/                         # Benchmark Datasets
│   ├── stl/
│   │   └── bracket.stl           # Benchmark EngineeringBracket (20 facets)
│   └── csv/
│       └── temperatures.csv      # ANSYS nodal temperature cloud (108 nodes)
│
├── tests/                        # Automated unit & verification test suites
│   ├── test_geometry.cpp         # 9/9 Geometry & CSV tests
│   ├── test_thermal.cpp          # 6/6 Thermal FEA solver & convergence tests
│   ├── test_performance.cpp      # 4/4 Thread pool & numerical verification tests
│   ├── test_hardware.cpp         # 5/5 Hardware adapter & fault recovery tests
│   └── test_neural.cpp           # 5/5 Manual backpropagation & gradient tests
│
├── results/                      # Output verification plots & screenshots
│   └── plots/
│       ├── geometry_verified.png
│       ├── thermal_verified.png
│       ├── perf_verified_full.png
│       ├── hw_verified_full.png
│       ├── neural_verified_full.png
│       └── reports_verified_full.png
│
├── docs/                         # Engineering reports, architecture & derivations
│   ├── ARCHITECTURE.md
│   ├── DATA_PROVENANCE.md
│   ├── PERFORMANCE_LAB.md
│   ├── HARDWARE_INTEGRATION.md
│   └── MANUAL_BACKPROP_DERIVATION.md
│
├── CMakeLists.txt                # Root CMake build configuration
├── build.bat                     # Automated Windows build script
├── Launch.exe                    # Win32 One-Click Root Launcher
├── workbench_server.exe          # Native C++ Backend Server
└── README.md                     # Project documentation & evaluator guide
```

---
*Developed for PASS SCIENCE WORKS R&D Internship 2026–2027 Evaluation.*
