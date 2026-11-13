# UNIFIED ENGINEERING WORKBENCH
## Systems Architecture & Technical Specification Document

### 1. Architectural Philosophy
The Unified Engineering Workbench is engineered around three core tenets:
1. **Computational Purity**: All mission-critical numerical computing, matrix assembly, equation solving, file parsing, and hardware mock telemetry are executed in compiled ISO C++17.
2. **Deterministic Reproducibility**: Numerical solvers and multicore worker batches use strict seeded workloads, verifying that serial and multithreaded outputs agree within machine precision ($\le 10^{-8} \text{ }^\circ\text{C}$).
3. **Zero-Configuration Evaluation**: Evaluators receive a completely self-contained distribution where `Launch.exe` initializes the backend and opens the user interface with zero terminal commands, zero Python runtime, zero npm packages, and zero internet connection.

---

### 2. Subsystem Architecture

```
+---------------------------------------------------------------------------------------+
|                                    USER INTERFACE                                     |
|  - HTML5 / CSS3 Dark Engineering Theme                                                |
|  - Three.js (r128) WebGL CAD Visualizer + OrbitControls                               |
|  - Plotly.js (v2.32) Scientific Graphing Engine                                       |
|  - ES6 Modular Controllers (api.js, app.js)                                           |
+---------------------------------------------------------------------------------------+
                                          |
                      JSON REST IPC over Loopback HTTP (127.0.0.1:8080)
                                          |
+---------------------------------------------------------------------------------------+
|                                  C++17 BACKEND ENGINE                                 |
|                                                                                       |
|  [ HttpServer (cpp-httplib) ]                                                         |
|     - REST API Endpoints (/api/health, /api/system/info, /api/logs, /api/shutdown)     |
|     - Static Asset Router (MIME-typed file delivery)                                  |
|                                                                                       |
|  [ Core Services ]                                                                    |
|     - Thread-safe Logger (in-memory ring buffer + stdout)                             |
|     - Win32 Hardware Telemetry Provider (CPUID, Physical/Logical Cores, RAM)          |
|                                                                                       |
|  [ Computational Modules ]                                                            |
|     - Task B: Geometry Engine (ASCII STL Parser + ANSYS CSV Cloud Engine)             |
|     - Task C: 1D Thermal FEA Solver (Linear Elements, Dirichlet, Gauss Elimination)   |
|     - Task D: Multicore Benchmark (Bounded std::thread Worker Pool)                   |
|     - Task E: Hardware Bus (Mock MCU, FPGA, JTAG Adapters with State Machine)          |
|     - Task F: Neural Engine (Analytical Backprop + Central Difference Gradient Check) |
+---------------------------------------------------------------------------------------+
                                          |
                                    Win32 Subsystem
                                          |
+---------------------------------------------------------------------------------------+
|                             WIN32 LAUNCHER (Launch.exe)                               |
|  - Direct executable entry point                                                      |
|  - Automatically spawns workbench_server.exe                                          |
|  - Opens default web browser to local HTTP endpoint                                   |
+---------------------------------------------------------------------------------------+
```

---

### 3. Concurrency & Memory Management
- **Threading Model**: `std::thread` is utilized directly for backend worker pools and the asynchronous HTTP server event loop.
- **Synchronization**: Thread safety across shared logging and telemetry caches is guaranteed via RAII `std::lock_guard<std::mutex>`.
- **Zero Memory Leaks**: All resources follow strict Modern C++ RAII semantics (`std::unique_ptr`, `std::vector`, standard containers), avoiding manual raw pointer allocations.
- **Network Stack**: Win32 Winsock (`ws2_32.lib`) and Cryptography (`crypt32.lib`) libraries are statically linked into the executable.

---

### 4. IPC Data Contract & Protocol Definition

#### Endpoint Matrix
| Route | Method | Payload / Parameters | Response Structure | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `/api/health` | `GET` | None | `{"status":"ok", "service":"...", "version":"..."}` | Liveness & latency probe |
| `/api/system/info` | `GET` | None | `{"cpu_model":"...", "physical_cores":8, "ram_gb":16.0, ...}` | Hardware topology |
| `/api/logs` | `GET` | `limit` (int, default 50) | `{"count":N, "logs":[{"timestamp":"...", "level":"...", "message":"..."}]}` | Live console stream |
| `/api/system/shutdown` | `POST` | None | `{"status":"shutting_down"}` | Clean termination |
| `/*` | `GET` | File path | Static assets (HTML, CSS, JS, STL, CSV) | Embedded UI delivery |

---

### 5. Verification & Quality Assurance Strategy
1. **Zero-Flake Unit Testing**: Every numerical module is accompanied by an automated C++ test executable validating boundary conditions and mathematical tolerances.
2. **Analytical Comparison**: The FEA solver is validated directly against the exact closed-form solution:
   $$T_{\text{exact}}(x) = 100 - 80\frac{x}{L} + \frac{q}{2k}x(L-x)$$
3. **Gradient Verification**: The neural backpropagation engine verifies analytical derivatives against symmetric finite differences:
   $$\frac{\partial J}{\partial \theta} \approx \frac{J(\theta + \epsilon) - J(\theta - \epsilon)}{2\epsilon}, \quad \epsilon = 10^{-6}$$
   with an acceptance threshold of $| \text{diff} | \le 10^{-5}$.
4. **Deterministic Parallelism**: Worker pool benchmarks guarantee identical outputs between single-threaded and multithreaded runs with maximum temperature deviation $\le 10^{-8} \text{ }^\circ\text{C}$.

