#include "geometry/StlParser.hpp"
#include "geometry/CsvParser.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <iomanip>

using namespace workbench::geometry;

int main() {
    std::cout << "========================================================\n";
    std::cout << "  REAL-SCALE PARSER MEASUREMENT AND EMPIRICAL AUDIT     \n";
    std::cout << "========================================================\n\n";

    // 1. Generate Real ASCII STL
    const int facetCount = 60000;
    std::string stlPath = "data/stl/test_scale_60k.stl";
    std::filesystem::create_directories("data/stl");
    {
        std::ofstream stlOut(stlPath, std::ios::binary);
        stlOut << "solid BenchmarkScaleTestSolid\n";
        for (int i = 0; i < facetCount; ++i) {
            float z = static_cast<float>(i) * 0.001f;
            stlOut << "  facet normal 0.000000 0.000000 1.000000\n"
                   << "    outer loop\n"
                   << "      vertex 0.000000 0.000000 " << z << "\n"
                   << "      vertex 1.000000 0.000000 " << z << "\n"
                   << "      vertex 0.500000 1.000000 " << z << "\n"
                   << "    endloop\n"
                   << "  endfacet\n";
        }
        stlOut << "endsolid BenchmarkScaleTestSolid\n";
    }

    uintmax_t stlBytes = std::filesystem::file_size(stlPath);
    double stlMb = static_cast<double>(stlBytes) / (1024.0 * 1024.0);

    // Benchmark C++ STL Parser
    auto t0 = std::chrono::steady_clock::now();
    StlParseResult stlRes = StlParser::parseAsciiFile(stlPath);
    auto t1 = std::chrono::steady_clock::now();
    double stlMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "[STL PARSE MEASUREMENT]\n";
    std::cout << "  File:           " << stlPath << "\n";
    std::cout << "  Exact Size:     " << stlBytes << " bytes (" << std::fixed << std::setprecision(2) << stlMb << " MB)\n";
    std::cout << "  Status:         " << (stlRes.success ? "SUCCESS" : "FAILED") << "\n";
    std::cout << "  Facets:         " << stlRes.geometry.triangleCount << " triangles\n";
    std::cout << "  Vertices:       " << stlRes.geometry.vertexCount << " coords\n";
    std::cout << "  Parse Duration: " << std::fixed << std::setprecision(1) << stlMs << " ms\n\n";

    // 2. Generate Real ANSYS CSV
    const int nodeCount = 250000;
    std::string csvPath = "data/csv/test_scale_250k.csv";
    std::filesystem::create_directories("data/csv");
    {
        std::ofstream csvOut(csvPath, std::ios::binary);
        csvOut << "node_id,x_m,y_m,z_m,temperature_C\n";
        for (int i = 1; i <= nodeCount; ++i) {
            float x = 0.050f * (static_cast<float>(i % 100) / 100.0f);
            float y = 0.030f * (static_cast<float>((i / 100) % 100) / 100.0f);
            float z = 0.080f * (static_cast<float>(i) / static_cast<float>(nodeCount));
            float temp = 35.0f + 115.0f * (z / 0.080f);
            csvOut << i << "," << std::fixed << std::setprecision(6)
                   << x << "," << y << "," << z << "," 
                   << std::setprecision(2) << temp << "\n";
        }
    }

    uintmax_t csvBytes = std::filesystem::file_size(csvPath);
    double csvMb = static_cast<double>(csvBytes) / (1024.0 * 1024.0);

    // Benchmark C++ CSV Parser
    t0 = std::chrono::steady_clock::now();
    CsvParseResult csvRes = CsvParser::parseFile(csvPath);
    t1 = std::chrono::steady_clock::now();
    double csvMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "[CSV PARSE MEASUREMENT]\n";
    std::cout << "  File:           " << csvPath << "\n";
    std::cout << "  Exact Size:     " << csvBytes << " bytes (" << std::fixed << std::setprecision(2) << csvMb << " MB)\n";
    std::cout << "  Status:         " << (csvRes.success ? "SUCCESS" : "FAILED") << "\n";
    std::cout << "  Parsed Nodes:   " << csvRes.cloud.nodes.size() << " nodes\n";
    std::cout << "  Temp Range:     [" << csvRes.cloud.minTemperature << " C, " << csvRes.cloud.maxTemperature << " C]\n";
    std::cout << "  Parse Duration: " << std::fixed << std::setprecision(1) << csvMs << " ms\n\n";

    // Clean up temporary test files so they don't bloat packaging
    std::filesystem::remove(stlPath);
    std::filesystem::remove(csvPath);
    std::cout << "[CLEANUP] Removed temporary test artifacts: " << stlPath << ", " << csvPath << "\n";

    return (stlRes.success && csvRes.success) ? 0 : 1;
}
