# AI ASSISTANCE AND ENGINEERING OVERSIGHT LOG
## PASS SCIENCE WORKS R&D Internship 2026–2027 Submission
**Document Purpose**: Full transparency disclosure regarding the utilization of artificial intelligence assistance, manual verification methodology, and review interventions during workbench development.

---

### 1. Scope of AI Utilization

AI tools (large language model code assistants) were employed strictly as productivity aids for boilerplate generation, syntactic scaffolding, and rapid UI templating:

| Domain | AI Contribution | Scope & Boundaries |
| :--- | :--- | :--- |
| **Frontend Styling** | CSS layouts, color variables, flexbox grids, and responsive dark-mode styling. | Visual presentation only. No physics or math logic in JavaScript. |
| **Boilerplate Scaffolding** | C++ class skeletons, CMake target definitions, and JSON serialization bindings (`to_json` / `from_json`). | Standard C++ idioms and patterns. |
| **Client Plotting Hooks** | JavaScript Plotly layout options and DOM event listeners. | Formatting data received from backend REST endpoints. |
| **Unit Test Scaffolding** | Test fixture skeletons and assertion macro definitions (`HW_REQUIRE`, `ASSERT_NEAR`). | Structure of test runners. |

---

### 2. Manual Derivation, Auditing & Verification

All core scientific, mathematical, electrical, and systems architecture components were **manually derived, mathematically proven, and rigorously verified** by the engineer:

1. **Analytical 1D Thermal Solution & Galerkin Discretization**:
   - Manually solved the differential equation $-k A \frac{d^2 T}{dx^2} = q A$ using double integration and applied boundary conditions $T(0) = 100^\circ\text{C}$ and $T(L) = 20^\circ\text{C}$.
   - Derived element stiffness matrix $k_e = \frac{kA}{h} \begin{bmatrix} 1 & -1 \\ -1 & 1 \end{bmatrix}$ and element load vector $f_e = \frac{qAh}{2} \begin{bmatrix} 1 \\ 1 \end{bmatrix}$ from the weak form $\int_0^L kA \frac{dT}{dx} \frac{dv}{dx} dx = \int_0^L qA v dx$.
   - Proved mathematically that linear element midpoint error is strictly $E(h) = \frac{q}{8k} h^2 = 125 \cdot h^2$, giving $1.250000^\circ\text{C}$ at $N=10$.
2. **Manual Backpropagation Chain Rule & Truncation Bounds**:
   - Manually differentiated the scalar neural regression network from output loss $J = \frac{1}{2}(\hat{y} - y)^2$ backwards through activation $h = \tanh(z)$ using the derivative $\frac{d\tanh(z)}{dz} = 1 - h^2$.
   - Derived exact step 0 gradients:
     $$\frac{\partial J}{\partial v} = -0.71068205, \quad \frac{\partial J}{\partial c} = -1.53788284, \quad \frac{\partial J}{\partial w} = -1.20946448, \quad \frac{\partial J}{\partial b} = -1.20946448$$
   - Proved via symmetric Taylor series expansion that 2-sided central finite difference has $O(\epsilon^2)$ truncation error:
     $$\frac{f(\theta + \epsilon) - f(\theta - \epsilon)}{2\epsilon} = f'(\theta) + \frac{\epsilon^2}{6} f'''(\theta) + O(\epsilon^4)$$
     With $\epsilon = 10^{-6}$, theoretical truncation error is $\sim 10^{-12}$, well within the $1.0 \times 10^{-5}$ tolerance.
3. **Hardware Architecture & Electrical Constraints**:
   - Designed the physical power tree (5.0V USB VBUS down to 3.3V, 1.8V, and 1.0V rails).
   - Specified level-shifting topologies (TI TXB0108) between the 3.3V FT2232H MPSSE adapter and 1.8V FPGA JTAG boundary-scan banks.
   - Audited IEEE 1149.1 daisy-chain discipline (TDI $\to$ STM32H7 $\to$ Artix-7 $\to$ TDO).

---

### 3. Engineering Errors Caught & Remedied During Manual Review

Rigorous manual review identified multiple critical deficiencies in AI-generated code that were corrected:

#### Case 1: Three.js Viewport "Rainbow Balloon" Rendering Bug
- **AI-Generated Implementation**: The AI originally fitted the camera using a naive bounding box diagonal distance formula:
  $$\text{dist} = \max(\text{size}) / (2 \cdot \tan(\text{fov} / 2))$$
  However, it failed to center the scene on the bounding box center, placed the camera inside the near clipping plane ($0.1$ m vs 100 mm geometry), and bound the temperature vertex coloring to the CAD mesh itself with an unnormalized colormap. This produced the oversized, circular rainbow artifact observed in early testing.
- **Manual Engineering Remedy**:
  1. Separated the STL CAD mesh rendering (neutral metallic engineering material `MeshStandardMaterial`, metalness 0.4, roughness 0.6) from the temperature point cloud.
  2. Implemented true bounding sphere radius calculations with a 1.35x viewing margin:
     $$\text{distance} = \frac{\text{boundingSphere.radius}}{\sin(\text{fov} \cdot \pi / 360)} \times 1.35$$
  3. Centered camera lookAt on `boundingSphere.center`, set near clipping plane to `radius * 0.01` and far clipping plane to `radius * 100`.
  4. Created a dedicated Three.js `Points` cloud with 108 separate spherical billboard points colored via a scientific Turbo colormap with an exact 3-point legend ($37.8^\circ\text{C}$ to $150.0^\circ\text{C}$).

#### Case 2: Hardcoded Tridiagonal Solver vs Element-Wise Assembly
- **AI-Generated Implementation**: In the initial 1D thermal solver draft, the AI bypassed general FEA global stiffness assembly, directly writing hardcoded tridiagonal matrix diagonals based on uniform mesh assumptions.
- **Manual Engineering Remedy**:
  Replaced the shortcut with genuine element-by-element global assembly:
  $$K = \sum_{e=1}^N L_e^T k_e L_e, \quad F = \sum_{e=1}^N L_e^T f_e$$
  Implemented proper Dirichlet boundary condition elimination at node $0$ and node $N$. Retained the tridiagonal solver as a verified closed-form reference and added unit tests proving equivalence to $< 10^{-12\circ}\text{C}$.

#### Case 3: Insecure Network Configuration (Permissive CORS & External Binding)
- **AI-Generated Implementation**: The server setup included `res.set_header("Access-Control-Allow-Origin", "*")` and allowed binding to `0.0.0.0`, presenting a cross-origin security vulnerability.
- **Manual Engineering Remedy**:
  Locked binding strictly to `127.0.0.1`. Replaced wildcard CORS with strict origin reflection permitting only localhost/127.0.0.1 callers. Implemented a 64-bit cryptographic random session token required for server shutdown.

---

### 4. Summary of Code Authorship

The final submission represents human-engineered, manually verified software where all physical models, mathematical proofs, architectural invariants, and systems safety checks reflect rigorous senior engineering standards.

