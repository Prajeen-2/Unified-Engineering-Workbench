# Empirical Measurement & Verification Tools

This directory contains standalone, reproducible audit and measurement utilities used to obtain empirical benchmark data for the Unified Engineering Workbench:

1. **`measure_real_file_limits.cpp`**:
   - Generates and empirically parses real, valid ASCII STL files and ANSYS CSV temperature point cloud files using production C++ parsers (`StlParser::parseAsciiFile` and `CsvParser::parseFile`).
   - Measures exact byte sizes and parse latencies:
     - ASCII STL: $60,000\text{ facets}$ ($180,000\text{ vertices}$) at $11.05\text{ MB}$ ($11,589,903\text{ bytes}$), parsed in $375.6\text{ ms}$.
     - ANSYS CSV: $250,000\text{ nodes}$ at $9.53\text{ MB}$ ($9,997,636\text{ bytes}$), parsed in $524.3\text{ ms}$.

2. **`measure_startup_timing.js`**:
   - Automates high-resolution process spawn and browser UI readiness measurements via the Chrome DevTools Protocol (CDP) on Microsoft Edge.
   - Measures:
     - Backend Process Spawn to Winsock `/api/health` 200 OK: $30.4\text{ ms}$ (mean of 3 runs: 25.3 ms – 32.7 ms).
     - Browser Navigation Timing (`domContentLoadedEventEnd - navigationStart`): $882\text{ ms}$ (DOM interactive: 668 ms, window load: 886 ms).
     - Full End-to-End Launch-to-Ready: $\approx 913\text{ ms}$ ($< 1\text{ second}$).
   - Automatically captures full-viewport screenshot saved to `results/plots/startup_timing_measured.png`.

3. **`test_thermal_ui.js`**:
   - End-to-end headless browser test verifying that the Thermal Analysis view starts in a clean `AWAITING RUN` state with `--` placeholders before running.
   - Tests that clicking "Run Simulation" solves the problem dynamically.
   - Tests that modifying input parameters ($T_0, T_L, q, k, L, N$) transitions UI to `PARAMETERS MODIFIED • CLICK RUN`.
   - Tests that re-running computes the custom analytical and numerical values dynamically (e.g. $T_0=150^\circ\text{C}, q=20000\,\text{W/m}^3 \to T_{\max}=338.00^\circ\text{C}, \text{Error}_{\max}=2.500000^\circ\text{C}$).
