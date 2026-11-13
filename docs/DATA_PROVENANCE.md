# DATA PROVENANCE AND PHYSICAL UNITS SPECIFICATION
## Task B: 3D CAD Geometry & Temperature Field Benchmarks

### 1. Synthetic STL Geometry: `engineering_bracket.stl`

- **File Path**: `data/stl/engineering_bracket.stl`
- **File Format**: Standard ASCII Stereolithography (STL)
- **Geometry Description**: Watertight, manifold structural L-bracket comprising a horizontal mounting plate and a vertical load-bearing flange.
- **Physical Dimensional Units**: Meters ($m$)
- **Spatial Bounds**:
  - $X \in [0.000, 0.100] \text{ m}$ (100 mm overall length)
  - $Y \in [0.000, 0.050] \text{ m}$ (50 mm width)
  - $Z \in [0.000, 0.080] \text{ m}$ (80 mm overall height)
- **Mesh Topology**:
  - Total Triangular Facets: $20$
  - Total Vertex Coordinates: $60$ (3 per facet)
  - Outward Surface Normals: Unit vectors adhering to the right-hand rule
  - Surface Area: $0.02715 \text{ m}^2$
- **Generation Methodology**: Synthesized directly in ISO C++ conforming to the ASTM STL specification.

---

### 2. Synthetic Temperature Field: `synthetic_thermal_field.csv`

- **File Path**: `data/csv/synthetic_thermal_field.csv`
- **File Format**: ANSYS Mechanical / APDL compatible comma-separated values (CSV)
- **Required Header Columns**: `node_id,x_m,y_m,z_m,temperature_C`
- **Physical Units**:
  - `node_id`: Non-negative integer index (dimensionless, unique)
  - `x_m`: Cartesian X-coordinate in meters ($m$)
  - `y_m`: Cartesian Y-coordinate in meters ($m$)
  - `z_m`: Cartesian Z-coordinate in meters ($m$)
  - `temperature_C`: Local thermal temperature in degrees Celsius ($^\circ\text{C}$)
- **Thermal Governing Equation & Physical Model**:
  Represents steady-state conductive-convective heat dissipation through an isotropic structural alloy bracket ($k = 45 \text{ W/(m}\cdot\text{K)}$).
  The base boundary at $x = 0$ is in contact with an isothermal thermal reservoir ($T_{\text{source}} = 150.0^\circ\text{C}$), cooling asymptotically toward ambient air ($T_{\infty} = 25.0^\circ\text{C}$) with exponential longitudinal decay and transverse Biot-dependent gradients:
  $$T(x,y,z) = 25.0 + 125.0 \cdot e^{-20.0 x} \left[0.8 + 0.2 \cos\left(2\pi \frac{y - 0.025}{0.05}\right)\right] - 5.0 \left(\frac{z}{0.015}\right)$$
- **Dataset Metrics**:
  - Total Validated Nodes: $108$
  - Minimum Temperature: $37.76^\circ\text{C}$ (Tip extremity)
  - Maximum Temperature: $150.00^\circ\text{C}$ (Root heat source)
  - Mean Field Temperature: $80.24^\circ\text{C}$
  - Non-finite rejection: Tested and validated zero $NaN$, zero $\pm\infty$.

