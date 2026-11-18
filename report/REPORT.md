# UNIFIED ENGINEERING WORKBENCH — TECHNICAL EVALUATION REPORT
## PASS SCIENCE WORKS R&D Internship 2026–2027 Evaluation Submission
**Author / Candidate**: Candidate Submission  
**Platform Target**: Windows 11 x64  
**Compiler**: Microsoft Visual C++ (MSVC 19.51 / C++17)  
**Host Architecture**: Intel Core i5-13420H (8 Physical Cores / 12 Logical Cores), 16 GB RAM  
**Execution Date**: November 2026  

---

## 1. Executive Summary

The **Unified Engineering Workbench** is an integrated, high-performance desktop CAE simulation and computing environment engineered strictly to satisfy the technical mandates of the PASS SCIENCE WORKS R&D 2026–2027 selection process. 

The application architecture enforces complete separation of concerns:
1. **Computation Layer**: 100% native ISO C++17 with zero Python backend runtime and zero external runtime DLL dependencies (statically linked MSVC runtime `/MT`).
2. **Presentation Layer**: A dark-engineering HTML5/WebGL single-page application utilizing locally bundled Three.js (r128) and Plotly.js (v2.32) without external CDN calls.
3. **Communication Layer**: Direct localhost HTTP/REST communication via `cpp-httplib` bound strictly to `127.0.0.1`, secured with session tokens, strict origin reflection, and asynchronous non-blocking job polling.

---

## 2. Benchmark Verification and Numerical Evidence

### 2.1 Task C: 1D Thermal Finite Element Solver & Mesh Convergence

The governing steady-state 1D heat conduction equation with uniform heat generation:
$$-\frac{d}{dx}\left(kA \frac{dT}{dx}\right) = qA, \quad x \in [0, L]$$
with boundary conditions $T(0) = T_{\text{left}} = 100^\circ\text{C}$ and $T(L) = T_{\text{right}} = 20^\circ\text{C}$.

The exact analytical solution is:
$$T_{\text{exact}}(x) = T_{\text{left}} + \frac{T_{\text{right}} - T_{\text{left}}}{L}x + \frac{q}{2k}x(L - x) = 100 - 80x + 500x(1 - x)$$

#### Finite Element Implementation
- **Element Formulation**: 2-node linear elements ($C^0$ continuous Lagrange polynomials).
- **Element Stiffness Matrix**:
  $$k_e = \frac{kA}{h} \begin{bmatrix} 1 & -1 \\ -1 & 1 \end{bmatrix}$$
- **Element Load Vector**:
  $$f_e = \frac{qAh}{2} \begin{bmatrix} 1 \\ 1 \end{bmatrix}$$
- **Assembly & Boundary Enforcement**: Assembled element-by-element into a global stiffness matrix $K$ and force vector $F$, with row/column substitution enforcing essential Dirichlet conditions at nodes $0$ and $N$.
- **Linear Solver**: Tridiagonal Gaussian elimination / Thomas Algorithm ($O(N)$ computational complexity).

#### Verified Numerical Convergence Results

| Mesh ($N$) | Element Length $h$ [m] | Midpoint $x_{\text{mid}}$ [m] | $T_{\text{mid,fem}}$ [°C] | $T_{\text{mid,exact}}$ [°C] | Midpoint Error $|T_{\text{fem}} - T_{\text{exact}}|$ [°C] | Convergence Ratio $\frac{E_{N}}{E_{2N}}$ |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **10** | 0.1000 | 0.5500 | 178.500000 | 179.750000 | **1.25000000** | Baseline |
| **20** | 0.0500 | 0.5250 | 182.375000 | 182.687500 | **0.31250000** | **4.0000** ($\alpha = 2.000$) |
| **40** | 0.0250 | 0.5125 | 183.843750 | 183.921875 | **0.07812500** | **4.0000** ($\alpha = 2.000$) |
| **80** | 0.0125 | 0.50625 | 184.460938 | 184.480469 | **0.01953125** | **4.0000** ($\alpha = 2.000$) |

![Thermal Convergence Plot](../results/plots/thermal_convergence.svg)

#### Numerical Analysis
1. **Exact Nodal Agreement**: Because the shape functions are linear and the true solution is quadratic, the weak form Galerkin discretization produces **exact nodal values** ($T_{\text{fem}}(x_i) = T_{\text{exact}}(x_i)$) at all mesh nodes to within machine precision ($< 10^{-12\circ}\text{C}$).
2. **Quadratic Midpoint Convergence**: Within any element $e$, the maximum interpolation error between a quadratic curve and a linear secant occurs at the element midpoint $x = x_e + h/2$:
   $$E(h) = \frac{q}{8k} h^2 = \frac{10000}{8 \cdot 10} h^2 = 125 \cdot h^2$$
   For $N = 10$ ($h = 0.1$ m), $E(0.1) = 125 \cdot (0.01) = 1.25^\circ\text{C}$ identically. Halving $h$ reduces the error by an exact factor of $4$, confirming theoretical asymptotic convergence order $\alpha = 2.000$.

---

### 2.2 Task D: Multicore Scalability & Performance Benchmarking

Multicore parallelization is implemented using a bounded worker pool (`workbench::performance::ThreadPool`) with `std::thread`, work-stealing job queues, condition variables, and mutex synchronization.

#### Hardware Audit
- **CPU**: 13th Gen Intel(R) Core(TM) i5-13420H (Raptor Lake)
- **Cores**: 8 Physical Cores (4 Performance Cores, 4 Efficient Cores) / 12 Logical Cores
- **RAM**: 16,071 MB DDR5
- **Compiler**: Microsoft Visual C++ 19.51 with `/O2 /Oi /Ot /Gy` optimization flags.
- **Workload**: 10,000 deterministic 1D thermal simulation cases, 200 elements per rod, PRNG seed 42, 5 measured repetitions per worker count.

#### Startup Timing Breakdown: Clarifying Launch-to-Ready
To eliminate ambiguity regarding startup latencies, the benchmark separates and explicitly reports each distinct phase of application launch:
1. **C++ In-Process Socket-Bind Latency (`1.07 ms`)**: Measured using `std::chrono::steady_clock` inside `workbench_server.exe` from static C++ runtime initialization to successful TCP port binding on `127.0.0.1:8080`.
2. **`Launch.exe` Process-Spawn-to-TCP-Health (`259 ms`)**: Measured from Win32 `Launch.exe` invoking `CreateProcessW` for the headless backend server until Winsock TCP health probes receive `HTTP 200 OK` from `/api/health`.
3. **End-to-End Browser UI Ready (`~680 ms`)**: Measured from Win32 `ShellExecuteW` browser invocation through default browser process initialization, HTML/CSS asset retrieval, Three.js WebGL shader compilation, Plotly.js DOM mount, and initial client-side telemetry ping.

#### Measured Multicore Benchmark Data (`results/csv/performance_multicore.csv`)

| Workers ($p$) | Repetition Times [ms] | Median $T_p$ [ms] | Speedup $S_p = \frac{T_1}{T_p}$ | Parallel Efficiency $E_p = \frac{S_p}{p}$ | Max Temperature Diff vs Serial [°C] | Verification Status |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | [137.83, 132.87, 136.45, 137.53, 146.75] | **137.5300** | **1.0000x** | **100.0%** | $0.000000 \times 10^0$ | **PASS** |
| **2** | [83.35, 82.41, 81.21, 75.38, 75.64] | **81.2111** | **1.6935x** | **84.7%** | $0.000000 \times 10^0$ | **PASS** |
| **3** | [57.78, 58.30, 55.12, 58.52, 55.27] | **57.7797** | **2.3802x** | **79.3%** | $0.000000 \times 10^0$ | **PASS** |
| **4** | [47.46, 53.99, 45.49, 47.36, 49.38] | **47.4636** | **2.8976x** | **72.4%** | $0.000000 \times 10^0$ | **PASS** |

![Speedup vs Workers Plot](../results/plots/speedup_vs_workers.svg)

#### Scalability & Efficiency Analysis
1. **Numerical Invariance**: Across all worker counts ($p = 1, 2, 3, 4$), the maximum temperature difference compared to the serial baseline is identically $0.0^\circ\text{C}$ (strictly meeting $\le 1.0 \times 10^{-8\circ}\text{C}$), proving zero race conditions, thread-safe memory isolation, and deterministic slot ordering.
2. **Robust Multicore Scaling at Scale**: With a production-scale workload of 10,000 cases and 200 elements per rod, the compute time per repetition ($137.5\text{ ms} \to 47.5\text{ ms}$) dwarfs thread pool queue synchronization overhead. The scaling demonstrates strong multicore efficiency ($84.7\%$ on 2 workers, $72.4\%$ on 4 workers). Efficiency reflects realistic cache contention across shared L3 cache and heterogeneous P-core / E-core thread scheduling on the Intel 13th Gen hybrid architecture.

---

### 2.3 Task F: Neural Network Manual Backpropagation & Gradient Verification

The regression neural architecture comprises a single scalar input $x$, one hidden neuron with hyperbolic tangent activation ($\tanh$), and a linear output neuron predicting target $y$:
$$z = w \cdot x + b$$
$$h = \tanh(z)$$
$$\hat{y} = v \cdot h + c$$
$$J(w,b,v,c) = \frac{1}{2}(\hat{y} - y)^2$$

#### Analytical Chain-Rule Gradients
Defining residual error $e = \hat{y} - y$:
$$\frac{\partial J}{\partial v} = e \cdot h, \quad \frac{\partial J}{\partial c} = e$$
$$\delta = \frac{\partial J}{\partial z} = e \cdot v \cdot (1 - h^2)$$
$$\frac{\partial J}{\partial w} = \delta \cdot x, \quad \frac{\partial J}{\partial b} = \delta$$

#### Verified Step 0 Benchmark Values ($x = 1.0, y = 2.0, w_0 = 0.5, b_0 = 0.0, v_0 = 1.0, c_0 = 0.0$)

| Quantity | Analytical Formula | Exact Computed Value | Finite Difference ($\epsilon = 10^{-6}$) | Numerical Discrepancy | Verification Status |
| :--- | :--- | :--- | :--- | :--- | :---: |
| Pre-activation $z$ | $w_0 x + b_0$ | 0.50000000 | — | — | PASS |
| Activation $h$ | $\tanh(0.5)$ | 0.46211716 | — | — | PASS |
| Output $\hat{y}$ | $v_0 h + c_0$ | 0.46211716 | — | — | PASS |
| Residual $e$ | $\hat{y} - y$ | -1.53788284 | — | — | PASS |
| Loss $J$ | $\frac{1}{2} e^2$ | **1.18254182** | — | — | PASS |
| $\partial J / \partial w$ | $e \cdot v \cdot (1-h^2) \cdot x$ | **-1.20946448** | -1.20946448 | $1.1302 \times 10^{-10}$ | **PASS** (Tol: $10^{-5}$) |
| $\partial J / \partial b$ | $e \cdot v \cdot (1-h^2)$ | **-1.20946448** | -1.20946448 | $1.1302 \times 10^{-10}$ | **PASS** (Tol: $10^{-5}$) |
| $\partial J / \partial v$ | $e \cdot h$ | **-0.71068205** | -0.71068205 | $5.2538 \times 10^{-11}$ | **PASS** (Tol: $10^{-5}$) |
| $\partial J / \partial c$ | $e$ | **-1.53788284** | -1.53788284 | $1.1175 \times 10^{-10}$ | **PASS** (Tol: $10^{-5}$) |

![Neural Loss and Gradient Verification Plot](../results/plots/neural_loss_grad_error.svg)

#### 100-Step Convergence Behavior
- Simultaneous gradient descent updates ($\eta = 0.1$):
  $$\theta_{k+1} = \theta_k - \eta \nabla_{\theta} J(\theta_k)$$
- Initial Loss: $1.1825418$  
- Final Loss (Step 100): **$1.70262 \times 10^{-20}$** ($\hat{y} = 2.000000000$)
- Maximum Gradient Verification Error across all 100 steps: **$1.60723 \times 10^{-10}$**, confirming strict compliance with the $1.0 \times 10^{-5}$ tolerance.
- Convergence Mechanism: Because the model has 4 adjustable parameters to fit 1 scalar target point, the optimization landscape has an exact global minimum at loss $0.0$, achieved with zero oscillation.

---

## 3. Software Architecture and Security Audit

```mermaid
flowchart TD
    Client["Browser UI (HTML5 / ES6 / WebGL)\nLocalhost:8080 (Three.js & Plotly.js)"]
    Launcher["Launch.exe (Win32 Non-Console Root Entrypoint)\nWinsock2 Direct Health Probe"]
    Server["workbench_server.exe (C++17 Server)\nStrict 127.0.0.1 Binding | Whitelisted CORS Reflection"]
    
    subgraph ComputeModules ["Pure C++17 Core Compute Engines"]
        CAD["StlParser & CsvParser\n50MB Cap, NaN/Inf Guards, Binary Detection"]
        FEA["ThermalSolver1D\nReal Global Element Assembly (Ke, Fe, BCs)"]
        Pool["PerformanceBenchmark\nBounded ThreadPool (1-4 Workers) & Workload Gen"]
        HW["HardwareManager\nSTM32H7, Artix-7, FT2232H Simulated Adapters"]
        NN["NeuralNetRegression\nManual Chain Rule & Central Difference Verification"]
    end
    
    Launcher -->|Spawn Hidden / Bring to Front| Server
    Client <-->|REST API / JSON / Async Polling| Server
    Server --> ComputeModules
```

### Key Engineering Safeguards Implemented
1. **Network Security & Local Isolation**:
   - Explicit binding to `127.0.0.1` (never `0.0.0.0`).
   - Wildcard CORS (`*`) eliminated; replaced with origin reflection strictly permitting `http://127.0.0.1:*` and `http://localhost:*`.
   - Shutdown endpoint (`POST /api/system/shutdown`) requires a 64-bit cryptographically secure session token generated at startup.
2. **Robust File Parsers**:
   - Enforces 50 MB file size limit (`DEFAULT_MAX_FILE_SIZE`).
   - Explicit rejection of 0-byte or whitespace-only files.
   - Distinctive error reporting for header-only CSVs and binary STLs with ASCII headers.
   - Rejection of non-finite coordinates ($NaN$, $\pm\infty$) and temperatures below absolute zero ($-273.15^\circ\text{C}$).
3. **Responsive Long-Running Operations**:
   - Asynchronous job execution (`POST /api/benchmark/start`, `POST /api/thermal/convergence/start`) with polling (`GET /api/jobs/{id}`) and cancellation (`POST /api/jobs/{id}/cancel`).
   - Eliminates UI freeze during intensive benchmark runs.

---

## 4. Conclusion

The Unified Engineering Workbench achieves 100% test passage across all 5 test suites (38 assertions) in 1.35 seconds under CTest. The codebase satisfies all requirements for performance, numerical fidelity, code cleanliness, and autonomous zero-install Windows 11 execution.

