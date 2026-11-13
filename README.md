# UNIFIED ENGINEERING WORKBENCH
## PASS SCIENCE WORKS R&D Internship 2026–2027 Selection Project

### Executive Overview
The **Unified Engineering Workbench** is an integrated, high-performance engineering simulation, CAD visualization, multicore benchmarking, embedded system telemetry, and neural computing platform designed for Windows 11 x64.

All numerical solvers, file parsers, concurrency managers, and hardware adapters are natively implemented in **ISO C++17** with zero external runtime dependencies. The user interface is built on a responsive, dark engineering frontend utilizing **Three.js** (for 3D CAD/temperature cloud rendering) and **Plotly.js** (for high-fidelity scientific data visualization). All communication between frontend and backend is handled through a lightweight, high-throughput REST architecture powered by **cpp-httplib** and **nlohmann/json**.

---

### Core Technology Stack

| Layer | Component | Specification |
| :--- | :--- | :--- |
| **Backend Language** | C++17 (MSVC / Clang / GCC) | ISO C++17 standard with multithreading (`std::thread`, `std::mutex`, `std::condition_variable`) |
| **Build Management** | CMake 3.16+ | Native multi-configuration CMake project targeting Visual Studio 2026 / MSVC x64 |
| **Frontend Framework** | HTML5 / CSS3 / ES6 | Premium Dark Engineering UI with zero external runtime dependencies |
| **3D CAD Visualization** | Three.js (r128) + OrbitControls | WebGL hardware-accelerated 3D facet rendering & point clouds |
| **Scientific Graphing** | Plotly.js (v2.32) | Interactive vector-grade scientific graphing and convergence plots |
| **JSON Serialization** | nlohmann/json (v3.11.3) | Header-only JSON processing standard |
| **HTTP / REST Server** | cpp-httplib (v0.18.3) | Header-only HTTP server running on Windows Sockets (`ws2_32`) |
| **Target Operating System**| Windows 11 x64 | Native Win32 desktop application with `Launch.exe` entrypoint |

---

### Zero-Friction Launch (Evaluator Experience)

To evaluate this application with **zero configuration, zero installation, zero admin rights, and 100% offline**:

1. Extract the submission ZIP file (`CandidateID_Workbench.zip`).
2. Double-click **`Launch.exe`** in the root directory.
3. The native launcher immediately initializes `workbench_server.exe` and opens the default web browser to the local workbench (`http://127.0.0.1:8080/`).

No command-line prompts, PowerShell scripts, npm installs, or compilers are required for evaluation.

---

### Source Compilation Instructions

If you wish to rebuild the C++ binaries from source on Windows:

#### Option A: One-Click Automated Build
Run the provided build script from the project root:
```bat
build.bat
```
This script automatically discovers the Visual Studio 2026 / MSVC x64 toolchain and CMake, configures an optimized Release build (`/O2`), and places `Launch.exe` and `workbench_server.exe` in the root and `build/bin/` directories.

#### Option B: Manual CMake & Ninja / Visual Studio Build
```powershell
# Open "x64 Native Tools Command Prompt for VS 2026"
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

#### Running the Backend with Custom Options
```powershell
# Run with custom port or headless mode
.\workbench_server.exe --port 8080 --no-browser
```

---

### Repository Layout

```
unified-engineering-workbench/
├── frontend/                     # Modern Dark Engineering UI
│   ├── css/
│   │   └── style.css             # High-contrast engineering stylesheet
│   ├── js/
│   │   ├── api.js                # REST API client & latency tracker
│   │   └── app.js                # UI controller & telemetry poller
│   ├── vendor/                   # Bundled offline libraries (Zero CDN dependency)
│   │   ├── three.min.js          # Three.js 3D library
│   │   ├── OrbitControls.js      # CAD camera orbit controller
│   │   └── plotly.min.js         # Scientific graphing engine
│   └── index.html                # Unified Workbench SPA
│
├── src/                          # C++17 Implementation
│   ├── core/                     # System Telemetry & Thread-safe Logger
│   │   ├── Logger.cpp
│   │   └── SystemInfo.cpp
│   ├── server/                   # REST HTTP Server & Static Asset Dispatcher
│   │   └── HttpServer.cpp
│   ├── launcher/                 # Win32 Zero-Config Application Launcher
│   │   └── launch.cpp
│   ├── geometry/                 # Task B (STL & CSV parsers) [Milestone B]
│   ├── thermal/                  # Task C (1D FEA solver) [Milestone C]
│   ├── performance/              # Task D (Worker pool benchmark) [Milestone D]
│   ├── hardware/                 # Task E (MCU/FPGA/JTAG mock adapters) [Milestone E]
│   ├── neural/                   # Task F (Manual backprop engine) [Milestone F]
│   └── main.cpp                  # Application backend entrypoint
│
├── include/                      # C++ Header Interfaces
│   ├── core/
│   │   ├── Logger.hpp
│   │   └── SystemInfo.hpp
│   └── server/
│       └── HttpServer.hpp
│
├── third_party/                  # Header-only C++ Libraries
│   ├── nlohmann/json.hpp         # JSON standard library
│   └── httplib/httplib.h         # cpp-httplib networking engine
│
├── data/                         # Test fixtures & mesh data
│   ├── stl/
│   └── csv/
│
├── tests/                        # Automated unit & verification test suites
├── results/                      # Output simulation CSVs and convergence plots
├── docs/                         # Engineering reports, architecture & derivations
│   └── ARCHITECTURE.md
│
├── CMakeLists.txt                # Root CMake build configuration
├── build.bat                     # Automated Windows build script
├── run.bat                       # Server execution helper
├── Launch.exe                    # Win32 One-Click Launcher
├── workbench_server.exe          # Native C++ Backend Server
├── README.md                     # Project documentation
└── .gitignore                    # Version control ignore rules
```

---

### Implementation Milestones & Roadmap

- **Milestone A: Project Foundation & Architecture** *(Completed)*
  - ISO C++17 modular backend architecture with MSVC x64 support.
  - Zero-dependency offline embedded frontend with dark engineering styling.
  - REST IPC infrastructure with real-time hardware telemetry and live log streaming.
  - Zero-click Windows `Launch.exe` launcher strategy.
- **Milestone B: Geometry & Temperature Cloud Viewer** *(Task B)*
  - High-performance C++ ASCII STL parser and ANSYS CSV temperature validator.
  - Interactive Three.js 3D viewport with rotation, panning, zoom, and colormapped point cloud inspection.
- **Milestone C: Thermal Finite Element Solver** *(Task C)*
  - Exact 1D steady-state heat equation FEM solver with linear elements.
  - Global stiffness assembly, Dirichlet elimination, and analytical error convergence ($N = 10, 20, 40, 80$).
- **Milestone D: Multicore Scalability Lab** *(Task D)*
  - Bounded C++ standard thread worker pool and deterministic workload dispatcher.
  - Speedup & efficiency benchmarking with strict numerical verification ($\le 10^{-8} \text{ }^\circ\text{C}$).
- **Milestone E: Embedded Hardware Integration** *(Task E)*
  - Microcontroller, FPGA, and JTAG hardware architecture design and C++ mock adapters with timeout recovery.
- **Milestone F: Analytical Backpropagation Engine** *(Task F)*
  - Exact manual chain-rule derivative calculation and finite-difference gradient verification ($\le 10^{-5}$).
- **Milestone G: Final Delivery & Submission Package**
  - Technical report, demonstration walkthrough, and candidate submission ZIP packaging.

---
*Developed for PASS SCIENCE WORKS R&D Internship 2026–2027 Evaluation.*

