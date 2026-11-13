# Unified Engineering Workbench - Dependency Lockfile

This file defines the exact pinned versions, source origins, licenses, and local repository paths for all embedded third-party dependencies used in this project.

| Package Name | Category | Pinned Version | License | Source / Upstream Repository | Local File Path | Integration Type |
|---|---|---|---|---|---|---|
| **cpp-httplib** | C++ Backend | `v0.18.3` | MIT | https://github.com/yhirose/cpp-httplib | `third_party/httplib.h` | Header-only (Single header) |
| **nlohmann/json** | C++ Backend | `v3.11.3` | MIT | https://github.com/nlohmann/json | `third_party/nlohmann/json.hpp` | Header-only (Single header) |
| **Three.js** | Frontend CAD | `r128` (0.128.0) | MIT | https://github.com/mrdoob/three.js | `frontend/vendor/three.min.js` | Vendored client runtime |
| **OrbitControls** | Frontend CAD | `r128` (0.128.0) | MIT | https://github.com/mrdoob/three.js | `frontend/vendor/OrbitControls.js` | Vendored client runtime |
| **Plotly.js** | Frontend Viz | `v2.32.0` | MIT | https://github.com/plotly/plotly.js | `frontend/vendor/plotly.min.js` | Vendored client runtime |

---

## C++ Compiler & Runtime Linkage Specifications
- **C++ Standard:** ISO C++17 (`/std:c++17`, `CMAKE_CXX_STANDARD 17`)
- **Runtime Library:** Static MSVC MultiThreaded (`/MT` in Release, `/MTd` in Debug via `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"`)
- **Dependencies Deployment:** Zero external dynamic link dependencies (no MSVCP140.dll / VCRUNTIME140.dll required on target Windows 11 systems). Standard OS system DLLs linked: `ws2_32.lib`, `crypt32.lib`, `wininet.lib`, `shell32.lib`, `user32.lib`, `kernel32.lib`.
- **Zero Python Runtime Dependency:** All numerical solvers, parsers, thread pools, and mock hardware protocols are natively compiled C++17.

