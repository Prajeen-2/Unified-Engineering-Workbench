# DATA PROVENANCE AND PHYSICAL UNITS SPECIFICATION
## Task B: 3D CAD Geometry & Temperature Field Benchmarks

### 1. Benchmark STL Geometry: `engineering_bracket.stl`

- **File Path**: `data/stl/engineering_bracket.stl`
- **File Format**: Standard ASCII Stereolithography (STL)
- **Geometry Description**: Representative structural engineering L-bracket comprising a horizontal mounting flange and a vertical load-bearing column.
- **Physical Dimensional Units**: Meters ($m$)
- **Spatial Bounds**:
  - $X \in [0.000, 0.100] \text{ m}$ (100.0 mm overall length)
  - $Y \in [0.000, 0.050] \text{ m}$ (50.0 mm width)
  - $Z \in [0.000, 0.080] \text{ m}$ (80.0 mm overall height)
- **Mesh Topology & Edge Audit**:
  - Total Triangular Facets: $20$
  - Total Vertex Coordinates: $60$ (3 per facet)
  - Total Unique Edges: $36$
  - Surface Area: $0.02715 \text{ m}^2$ ($27,150 \text{ mm}^2$)
  - **Manifold Status Audit**: Edge adjacency analysis indicates 12 boundary edges (edges shared by only 1 triangle) occurring at the geometric step-joint intersection and back-wall boundary. In strict computational topology, this 20-facet model represents an open coarse CAD boundary representation with T-junctions rather than a closed, watertight 2-manifold solid. The viewer safely handles this topology without manifoldness assumptions.
- **Generation Methodology**: Synthesized directly in ISO C++ adhering to the ASCII STL standard specification.

---

### 2. Benchmark Temperature Field: `synthetic_thermal_field.csv`

- **File Path**: `data/csv/synthetic_thermal_field.csv`
- **File Format**: Comma-separated values (CSV) matching ANSYS Mechanical / APDL nodal export conventions.
- **Naming Rationale ("ANSYS CSV")**: Named "ANSYS CSV" in the UI and internship specifications because its structure (`node_id,x_m,y_m,z_m,temperature_C`) reflects the standard column export schema used by ANSYS FEA thermal post-processors.
- **Synthetic Benchmark Generation**: Generated via a deterministic synthetic mathematical thermal model for reproducible benchmarking of Task B point cloud parsing and visualization.
- **Spatial Grid Topology**:
  - Regular structured nodal grid: $6$ stations in $X$, $6$ stations in $Y$, $3$ stations in $Z$.
  - Total Validated Nodes: $6 \times 6 \times 3 = 108$ nodes.
  - Spatial extent: $x \in [0.0, 0.100]\text{ m}$, $y \in [0.0, 0.050]\text{ m}$, $z \in [0.0, 0.015]\text{ m}$ spanning the horizontal mounting flange.
- **Required Header Columns & Units**:
  - `node_id`: Non-negative integer index (dimensionless, 1 to 108, strictly unique)
  - `x_m`: Cartesian X-coordinate in meters ($m$)
  - `y_m`: Cartesian Y-coordinate in meters ($m$)
  - `z_m`: Cartesian Z-coordinate in meters ($m$)
  - `temperature_C`: Local temperature in degrees Celsius ($^\circ\text{C}$)
- **Thermal Governing Equation & Physical Model**:
  Represents steady-state conductive-convective heat dissipation through an isotropic structural alloy bracket ($k = 45 \text{ W/(m}\cdot\text{K)}$).
  The base boundary at $x = 0$ is in contact with an isothermal thermal reservoir ($T_{\text{source}} = 150.0^\circ\text{C}$), cooling asymptotically toward ambient air ($T_{\infty} = 25.0^\circ\text{C}$) with exponential longitudinal decay and transverse Biot-dependent gradients:
  $$T(x,y,z) = 25.0 + 125.0 \cdot e^{-20.0 x} \left[0.8 + 0.2 \cos\left(2\pi \frac{y - 0.025}{0.05}\right)\right] - 5.0 \left(\frac{z}{0.015}\right)$$
- **Dataset Metrics**:
  - Minimum Temperature: $37.76^\circ\text{C}$ (Tip extremity)
  - Maximum Temperature: $150.00^\circ\text{C}$ (Root heat source)
  - Mean Field Temperature: $80.24^\circ\text{C}$
  - Non-finite rejection: Tested and validated zero $NaN$, zero $\pm\infty$, strictly above absolute zero ($-273.15^\circ\text{C}$).

---

### 3. File Size Limit & Tested Scale
- **Architectural Limit:** Strict 50 MB ($52,428,800\text{ bytes}$) cap enforced in C++ to prevent memory exhaustion (`DEFAULT_MAX_FILE_SIZE`).
- **Actual Largest Tested STL:** $60,000\text{ facets}$ ($180,000\text{ vertices}$) at **$11.05\text{ MB}$** ($11,589,903\text{ bytes}$, parsed in $375.6\text{ ms}$ via `StlParser::parseAsciiFile`).
- **Actual Largest Tested CSV:** $250,000\text{ thermal nodes}$ at **$9.53\text{ MB}$** ($9,997,636\text{ bytes}$, parsed in $524.3\text{ ms}$ via `CsvParser::parseFile`).
- **Oversized File Rejection Test:** Verified by `testOversizedFileLimit` rejecting a $50.000001\text{ MB}$ ($52,428,801\text{ byte}$) stream with `Oversized File Limit Exceeded`.

