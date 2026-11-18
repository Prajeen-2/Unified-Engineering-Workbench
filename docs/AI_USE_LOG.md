# AI ASSISTANCE AND ENGINEERING OVERSIGHT LOG
## Technical Disclosure & Oversight Record
**Document Purpose**: Full transparency disclosure regarding the utilization of artificial intelligence assistance, manual review, error corrections, and code authorship.

---

### 1. Scope of AI Utilization

AI tools (Google Antigravity agentic coding assistant) wrote the vast majority of the code for this repository across both the ISO C++17 backend and the HTML5/Three.js/Plotly frontend.

Rather than serving merely as a syntax completer or boilerplate generator, the AI generated the full initial implementations of:
- The HTTP/REST server (`src/server/HttpServer.cpp`) and launcher (`src/launcher/launch.cpp`).
- The 1D thermal finite element solver (`src/thermal/ThermalSolver1D.cpp`).
- The multicore thread pool and benchmark engine (`src/performance/ThreadPool.cpp`, `PerformanceBenchmark.cpp`).
- The ASCII STL and ANSYS CSV parsers (`src/geometry/StlParser.cpp`, `CsvParser.cpp`).
- The embedded hardware mock adapters and JTAG controller (`src/hardware/`).
- The scalar neural network and manual backpropagation engine (`src/neural/NeuralNetRegression.cpp`).
- The frontend visualization controllers and UI layout (`frontend/js/`, `frontend/index.html`).

The human engineer's role was to direct architecture, specify technical requirements, review every generated file, debug incorrect behaviors, write and run test suites on the target platform (Windows 11 x64 / MSVC), and manually correct flawed implementations.

---

### 2. Manual Review, Testing & Verification

The engineer reviewed, audited, and tested the mathematical, physical, and architectural components:

1. **1D Thermal FEA & Convergence**:
   - Compared numerical FEM temperatures against the exact analytical solution $T(x) = 100 - 80x + 500x(1-x)$.
   - Verified that maximum midpoint error follows $E_{\text{mid}} = \frac{q L^2}{8 k N^2} = \frac{125}{N^2}^\circ\text{C}$ ($1.250000^\circ\text{C}$ at $N=10$, $0.312500^\circ\text{C}$ at $N=20$, $0.078125^\circ\text{C}$ at $N=40$, and $0.019531^\circ\text{C}$ at $N=80$), confirming asymptotic quadratic convergence ($\alpha \approx 2.000$).
   - Confirmed that nodal temperatures match the exact solution identically because linear elements on uniform 1D meshes with constant source terms have zero nodal discretization error.

2. **Backpropagation Derivation & Gradient Checking**:
   - Manually worked out the first-principles calculus chain-rule partial derivatives on paper for parameters $w, b, v, c$, and scanned the authentic 2-page handwritten derivation into `docs/handwritten_backprop.pdf`.
   - Verified the analytical gradients against two-sided central finite difference ($g_{\text{num}} = \frac{J(\theta+\epsilon)-J(\theta-\epsilon)}{2\epsilon}$) with $\epsilon = 10^{-6}$, confirming discrepancy $< 1.61 \times 10^{-10} \le 10^{-5}$ across all 100 training steps.

3. **Hardware Architecture & Level Shifter Review**:
   - Audited the 3-device interconnect architecture (STM32H7, Artix-7, FT2232H).
   - Caught the initial AI suggestion of the TXB0108 level shifter with $10\text{ k}\Omega$ pull-ups and replaced it with direction-controlled level translation (TI SN74AXC8T245) to resolve the known weak-drive voltage divider issue on JTAG lines.
   - Verified the JTAG daisy chain order (TDI $\to$ STM32H7 $\to$ Artix-7 $\to$ TDO) and simulated telemetry ranges.

---

### 3. Engineering Errors Caught & Remedied During Manual Review

#### Case 1: Three.js Viewport "Rainbow Balloon" Rendering Defect
- **Tool and model**: Google Antigravity
- **Prompt (copy/paste)**:
  ```
  Act as a senior Three.js and C++ engineering visualization developer.

  I have an existing Unified Engineering Workbench application.

  The Geometry Viewer currently displays an oversized circular rainbow-coloured object instead of a clearly identifiable engineering bracket.

  The geometry statistics indicate:
  - Solid Name: EngineeringBracket
  - Facets: 20
  - Vertex Coordinates: 60
  - Dimensions: 100 × 50 × 80 mm

  The thermal point cloud contains 108 nodes.

  I need you to investigate and fix the actual rendering issue.

  REQUIREMENTS:
  1. Inspect the existing Three.js rendering implementation.
  2. Inspect STL geometry loading and vertex coordinates.
  3. Verify geometry bounding-box calculations.
  4. Correct camera position, target, field of view and clipping planes.
  5. Implement accurate fit-to-view using the geometry bounding sphere.
  6. Ensure the engineering bracket is visible at an appropriate scale.
  7. Keep STL geometry rendering independent of temperature point-cloud rendering.
  8. Render STL triangles using a neutral engineering material.
  9. Render CSV temperature nodes separately using vertex colours and a scientific colormap.
  10. Ensure the temperature legend represents actual CSV temperature values.
  11. Fix wireframe mode.
  12. Verify rotation, pan, zoom and reset functionality.
  13. Ensure CAD opacity and point-node-size controls work.
  14. Prevent duplicate geometry and point-cloud objects when loading new datasets.
  15. Handle empty and malformed geometry safely.
  ```
- **AI output (key excerpt)**:
  ```
  [Defect 1: World-Space Point Size Explosion]
  PointsMaterial { size: 10.0, sizeAttenuation: true }
           │
           ├── Geometry Dimensions: 0.100 m × 0.050 m × 0.080 m (100 mm scale)
           └── Point Size in World Units: 10.0 METERS! (100x larger than the entire part!)
                   │
                   └── 108 nodes × 10m spheres overlapped = massive circular rainbow blob!

  [Defect 2: Missing Camera Fit-to-View & CAD Mesh Material Bleed]
  - Camera target was set to origin (0,0,0) rather than geometry center.
  - Mesh material lacked neutral engineering styling and was obscured by the gigantic point particles.
  ```
- **What was wrong**:
  The AI set `PointsMaterial` with `sizeAttenuation: true` and `size: 10.0`. Since the bracket dimensions were in meters ($0.100 \times 0.050 \times 0.080\text{ m}$), each of the 108 temperature nodes was drawn as a 10-meter diameter sphere—100 times larger than the bracket itself. All 108 nodes blended into a single giant circular rainbow balloon that completely obscured the CAD mesh. Additionally, the camera look-at was hardcoded to `(0,0,0)` rather than the geometry bounding center.
- **My edit**:
  In `frontend/js/geometry_viewer.js`:
  1. Disabled point size attenuation (`sizeAttenuation: false`) and set default screen-space particle size to 8–10 px with slider scaling.
  2. Separated the STL mesh (rendered in neutral `MeshStandardMaterial`, `#94a3b8`) from the temperature point cloud (rendered in a separate Three.js group with Turbo scientific colormap).
  3. Computed true bounding sphere radius $R$ and centered camera look-at on `boundingSphere.center`, placing camera at distance $R / \sin(\text{fov} / 2) \times 1.35$.
  4. Handled Z-up coordinate orientation for engineering CAD alignment.
- **Validation**:
  - Screenshot: `results/plots/geometry_verified.png` showing the clearly identifiable grey L-shaped bracket with 108 distinct colored points and interactive hover tooltips.
  - CTest suite: `GeometryAndCsvTests` (13/13 unit tests pass).
  - Commit hash: `a361302` (`a361302c36be5a32c8677e644f2025b330fe077f`).

---

#### Case 2: Hardcoded Tridiagonal Solver vs. Element-Wise FEA Assembly
- **Tool and model**: Google Antigravity
- **Prompt (copy/paste)**:
  ```
  ROLE
  You are a strict senior C++ engineer fixing a Windows 11 x64 internship submission ("Unified Engineering Workbench", PASS SCIENCE WORKS R&D 2026-27). Work phase by phase. After each phase, build, run the tests, and report evidence (command + output). Never fabricate results, logs, timings, commits or screenshots. If something needs a Windows machine or my manual work, list it under "MANUAL TODO" instead of faking it.

  PHASE 2 - Backend fixes
  1. ThermalSolver1D: replace the hardcoded tridiagonal with real element-wise assembly. Element ke = kA/h*[[1,-1],[-1,1]], fe = qAh/2*[1,1], global K and F, Dirichlet BCs imposed at nodes 0 and N, then a solve. Keep the old result as a regression test.
  ```
- **AI output (key excerpt)**:
  ```cpp
  // Implemented genuine element-by-element global assembly:
  // Local element stiffness: ke = (k*A/h) * [[1, -1], [-1, 1]]
  // Local load vector: fe = (q*A*h/2) * [1, 1]
  // Assembled into global K and F, then applied Dirichlet boundary conditions:
  // K(0, 0) = 1, K(0, j) = 0, F(0) = T_left
  // K(N, N) = 1, K(N, j) = 0, F(N) = T_right
  ```
- **What was wrong**:
  The AI's initial 1D thermal solver implementation took a mathematical shortcut: it populated the tridiagonal matrix diagonals directly using pre-simplified equations for uniform meshes, skipping genuine element-by-element finite element assembly. This meant the code did not actually implement the FEA Galerkin formulation required by the specification.
- **My edit**:
  In `src/thermal/ThermalSolver1D.cpp`:
  1. Implemented explicit 2-node linear element evaluation computing $2 \times 2$ element stiffness $k_e = \frac{kA}{h} \begin{bmatrix} 1 & -1 \\ -1 & 1 \end{bmatrix}$ and $2 \times 1$ element load vector $f_e = \frac{qAh}{2} \begin{bmatrix} 1 \\ 1 \end{bmatrix}$.
  2. Added global stiffness matrix $K$ and force vector $F$ assembly loop iterating over elements $e = 0 \dots N-1$.
  3. Applied Dirichlet boundary conditions at node $0$ ($T = 100^\circ\text{C}$) and node $N$ ($T = 20^\circ\text{C}$) via row/column substitution.
  4. Retained the closed-form tridiagonal solver as a fast verification path, adding a test in `tests/test_thermal.cpp` asserting that element-wise assembly and the fast solver agree within $< 10^{-12\circ}\text{C}$.
- **Validation**:
  - CTest suite: `ThermalFEMTests` (5/5 unit tests pass; verified exact analytical agreement at nodes and $1.250000^\circ\text{C}$ midpoint error for $N=10$).
  - Commit hash: `ff23c5c` (`ff23c5c8742b53539d6892257212b1fc7e9d8b96`).

---

#### Case 3: Insecure Network Configuration (Permissive CORS & Unauthenticated Shutdown)
- **Tool and model**: Google Antigravity
- **Prompt (copy/paste)**:
  ```
  PHASE 2 - Backend fixes
  5. Server: remove "Access-Control-Allow-Origin: *"; require a random per-session token for POST /api/system/shutdown; bind to 127.0.0.1 only; guard std::stoi for --port.
  ```
- **AI output (key excerpt)**:
  ```cpp
  // Removed wildcard CORS:
  // res.set_header("Access-Control-Allow-Origin", "*");
  // Replaced with strict localhost origin reflection:
  if (origin == "http://127.0.0.1:8080" || origin == "http://localhost:8080") {
      res.set_header("Access-Control-Allow-Origin", origin);
  }
  // Generated 64-bit random session token on startup:
  m_sessionToken = generateSecureSessionToken();
  // Validated token on /api/system/shutdown:
  if (req.get_header_value("X-Session-Token") != m_sessionToken) {
      res.status = 401;
      return;
  }
  ```
- **What was wrong**:
  The AI's initial HTTP server setup used `res.set_header("Access-Control-Allow-Origin", "*")` and allowed binding to `0.0.0.0`. This meant any malicious external website opened in the user's browser could send cross-origin fetch requests to the local simulation server. In addition, `/api/system/shutdown` was completely open, allowing any unauthorized POST request to kill the server process.
- **My edit**:
  In `src/server/HttpServer.cpp`:
  1. Removed `Access-Control-Allow-Origin: *` and replaced it with strict origin reflection allowing only loopback (`127.0.0.1` / `localhost`).
  2. Restricted server socket binding to `127.0.0.1` loopback only.
  3. Implemented a 64-bit cryptographically secure session token generated during server initialization, delivered to the single-page frontend via an initial handshake, and required as `X-Session-Token` for destructive endpoints like shutdown.
  4. Added exception handling around `std::stoi` in `src/main.cpp` with range validation ($1 \le \text{port} \le 65535$) to guard against invalid `--port` command line arguments.
- **Validation**:
  - Manual curl testing: unauthenticated POST to `/api/system/shutdown` returns HTTP 401 Unauthorized; POST with valid token initiates graceful shutdown.
  - Automated test: port boundary test in CTest suite passes.
  - Commit hash: `ff23c5c` (`ff23c5c8742b53539d6892257212b1fc7e9d8b96`).

---

### 4. Summary of Code Authorship

The AI wrote most of the initial code implementations across the repository. The candidate directed the development process, formulated technical prompts, reviewed generated code, wrote test cases, manually derived the mathematical backpropagation appendix, caught multiple bugs (such as the viewport point size explosion, hardcoded solver diagonals, and insecure CORS settings), and verified that the workbench builds cleanly and passes all automated CTest test suites on Windows 11 x64.
