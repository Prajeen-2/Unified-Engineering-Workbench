#include "geometry/StlParser.hpp"
#include "geometry/CsvParser.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "[TEST FAILED] " << message << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

#define ASSERT_FALSE(condition, message) \
    do { \
        if (condition) { \
            std::cerr << "[TEST FAILED] " << message << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

bool testValidStlParsing() {
    std::string validStl = 
        "solid TestBox\n"
        "  facet normal 0.0 0.0 1.0\n"
        "    outer loop\n"
        "      vertex 0.0 0.0 1.0\n"
        "      vertex 1.0 0.0 1.0\n"
        "      vertex 1.0 1.0 1.0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0.0 0.0 1.0\n"
        "    outer loop\n"
        "      vertex 0.0 0.0 1.0\n"
        "      vertex 1.0 1.0 1.0\n"
        "      vertex 0.0 1.0 1.0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid TestBox\n";

    auto res = workbench::geometry::StlParser::parseAsciiString(validStl);
    ASSERT_TRUE(res.success, "Valid STL should parse successfully");
    ASSERT_TRUE(res.geometry.solidName == "TestBox", "Solid name should match");
    ASSERT_TRUE(res.geometry.triangleCount == 2, "Should have exactly 2 triangles");
    ASSERT_TRUE(res.geometry.vertexCount == 6, "Should have exactly 6 vertices");
    ASSERT_TRUE(std::abs(res.geometry.surfaceArea - 1.0) < 1e-6, "Unit square should have area 1.0");
    ASSERT_TRUE(std::abs(res.geometry.boundingBox.dimensions.x - 1.0) < 1e-6, "X dimension should be 1.0");
    ASSERT_TRUE(std::abs(res.geometry.boundingBox.dimensions.y - 1.0) < 1e-6, "Y dimension should be 1.0");
    std::cout << "  [PASS] testValidStlParsing\n";
    return true;
}

bool testInvalidStlHeader() {
    std::string invalidStl = "invalid_header 1 2 3\nfacet normal 0 0 1\nvertex 0 0 0\n";
    auto res = workbench::geometry::StlParser::parseAsciiString(invalidStl);
    ASSERT_FALSE(res.success, "STL missing 'solid' keyword must fail");
    std::cout << "  [PASS] testInvalidStlHeader: " << res.errorMessage << "\n";
    return true;
}

bool testMalformedStlVertices() {
    std::string truncatedStl = 
        "solid BrokenFacet\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid BrokenFacet\n";
    auto res = workbench::geometry::StlParser::parseAsciiString(truncatedStl);
    ASSERT_FALSE(res.success, "Facet with 2 vertices must be rejected");
    std::cout << "  [PASS] testMalformedStlVertices: " << res.errorMessage << "\n";
    return true;
}

bool testNonFiniteStlCoordinates() {
    std::string nanStl = 
        "solid NanTest\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0.0 nan 0.0\n"
        "      vertex 1.0 0.0 0.0\n"
        "      vertex 1.0 1.0 0.0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid NanTest\n";
    auto res = workbench::geometry::StlParser::parseAsciiString(nanStl);
    ASSERT_FALSE(res.success, "NaN coordinate in STL vertex must be rejected");
    std::cout << "  [PASS] testNonFiniteStlCoordinates: " << res.errorMessage << "\n";
    return true;
}

bool testValidCsvParsing() {
    std::string validCsv = 
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,100.0\n"
        "2,0.1,0.0,0.0,80.5\n"
        "3,0.2,0.0,0.0,65.2\n"
        "4,0.3,0.0,0.0,50.0\n";

    auto res = workbench::geometry::CsvParser::parseString(validCsv);
    ASSERT_TRUE(res.success, "Valid ANSYS CSV must parse successfully");
    ASSERT_TRUE(res.cloud.nodeCount == 4, "Must parse 4 nodes");
    ASSERT_TRUE(std::abs(res.cloud.minTemperature - 50.0) < 1e-6, "Min temp must be 50.0");
    ASSERT_TRUE(std::abs(res.cloud.maxTemperature - 100.0) < 1e-6, "Max temp must be 100.0");
    ASSERT_TRUE(std::abs(res.cloud.boundingBox.dimensions.x - 0.3) < 1e-6, "X span should be 0.3 m");
    std::cout << "  [PASS] testValidCsvParsing\n";
    return true;
}

bool testMissingCsvColumns() {
    std::string missingTempCol = 
        "node_id,x_m,y_m,z_m\n"
        "1,0.0,0.0,0.0\n";
    auto res = workbench::geometry::CsvParser::parseString(missingTempCol);
    ASSERT_FALSE(res.success, "CSV missing temperature_C must be rejected");
    ASSERT_TRUE(res.errorMessage.find("temperature_C") != std::string::npos, "Error must mention temperature_C");
    std::cout << "  [PASS] testMissingCsvColumns: " << res.errorMessage << "\n";
    return true;
}

bool testNonFiniteCsvCoordinates() {
    std::string nanCoordCsv = 
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,nan,0.0,50.0\n";
    auto res = workbench::geometry::CsvParser::parseString(nanCoordCsv);
    ASSERT_FALSE(res.success, "NaN coordinate in CSV must be rejected");
    std::cout << "  [PASS] testNonFiniteCsvCoordinates: " << res.errorMessage << "\n";
    return true;
}

bool testNonFiniteCsvTemperature() {
    std::string infTempCsv = 
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,inf\n";
    auto res = workbench::geometry::CsvParser::parseString(infTempCsv);
    ASSERT_FALSE(res.success, "Inf temperature in CSV must be rejected");
    std::cout << "  [PASS] testNonFiniteCsvTemperature: " << res.errorMessage << "\n";
    return true;
}

bool testBelowAbsoluteZeroCsv() {
    std::string absZeroCsv = 
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,-300.0\n";
    auto res = workbench::geometry::CsvParser::parseString(absZeroCsv);
    ASSERT_FALSE(res.success, "Temperature below -273.15 C must be rejected");
    std::cout << "  [PASS] testBelowAbsoluteZeroCsv: " << res.errorMessage << "\n";
    return true;
}

bool testEmptyFileRejection() {
    auto resStl = workbench::geometry::StlParser::parseAsciiString("");
    ASSERT_FALSE(resStl.success, "Empty STL string must be rejected");
    ASSERT_TRUE(resStl.errorMessage.find("empty") != std::string::npos, "Error message must state empty file");

    auto resStlWs = workbench::geometry::StlParser::parseAsciiString("   \n\t  \r\n ");
    ASSERT_FALSE(resStlWs.success, "Whitespace-only STL string must be rejected");

    auto resCsv = workbench::geometry::CsvParser::parseString("");
    ASSERT_FALSE(resCsv.success, "Empty CSV string must be rejected");
    ASSERT_TRUE(resCsv.errorMessage.find("empty") != std::string::npos, "Error message must state empty file");

    std::cout << "  [PASS] testEmptyFileRejection\n";
    return true;
}

bool testHeaderOnlyCsvRejection() {
    std::string headerOnly = "node_id,x_m,y_m,z_m,temperature_C\n";
    auto res = workbench::geometry::CsvParser::parseString(headerOnly);
    ASSERT_FALSE(res.success, "Header-only CSV without data rows must be rejected");
    ASSERT_TRUE(res.errorMessage.find("header-only CSV") != std::string::npos,
                "Error message must specify header-only CSV condition");
    std::cout << "  [PASS] testHeaderOnlyCsvRejection: " << res.errorMessage << "\n";
    return true;
}

bool testBinaryStlDetection() {
    // Binary STL with ASCII-like header: 80 bytes starting with "solid BinaryModel" followed by null bytes and binary data
    std::string binStl(120, '\0');
    std::string header = "solid BinaryModelCADExportHeaderWith80BytesPadding";
    std::copy(header.begin(), header.end(), binStl.begin());
    // Put non-zero binary bytes after 80-byte header
    binStl[80] = 0x02; binStl[81] = 0x00; binStl[82] = 0x00; binStl[83] = 0x00; // 2 triangles

    auto res = workbench::geometry::StlParser::parseAsciiString(binStl);
    ASSERT_FALSE(res.success, "Binary STL with null bytes must be rejected");
    ASSERT_TRUE(res.errorMessage.find("Detected binary STL format") != std::string::npos,
                "Error message must state binary STL format detected");
    std::cout << "  [PASS] testBinaryStlDetection: " << res.errorMessage << "\n";
    return true;
}

bool testOversizedFileLimit() {
    std::string smallContent = "solid Test\n  facet normal 0 0 1\n    outer loop\n      vertex 0 0 0\n      vertex 1 0 0\n      vertex 0 1 0\n    endloop\n  endfacet\nendsolid Test\n";
    // Enforce 10-byte limit
    auto resStl = workbench::geometry::StlParser::parseAsciiString(smallContent, 10);
    ASSERT_FALSE(resStl.success, "Content exceeding size limit must be rejected");
    ASSERT_TRUE(resStl.errorMessage.find("exceeds maximum allowable limit") != std::string::npos,
                "Error message must state size limit exceeded");

    auto resCsv = workbench::geometry::CsvParser::parseString("node_id,x,y,z,temperature\n1,0,0,0,25\n", 10);
    ASSERT_FALSE(resCsv.success, "CSV exceeding size limit must be rejected");
    ASSERT_TRUE(resCsv.errorMessage.find("exceeds maximum allowable limit") != std::string::npos,
                "Error message must state size limit exceeded");

    std::cout << "  [PASS] testOversizedFileLimit\n";
    return true;
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "   RUNNING TASK B GEOMETRY & CSV VERIFICATION TESTS     \n";
    std::cout << "========================================================\n";

    bool allPassed = true;
    allPassed &= testValidStlParsing();
    allPassed &= testInvalidStlHeader();
    allPassed &= testMalformedStlVertices();
    allPassed &= testNonFiniteStlCoordinates();
    allPassed &= testValidCsvParsing();
    allPassed &= testMissingCsvColumns();
    allPassed &= testNonFiniteCsvCoordinates();
    allPassed &= testNonFiniteCsvTemperature();
    allPassed &= testBelowAbsoluteZeroCsv();
    allPassed &= testEmptyFileRejection();
    allPassed &= testHeaderOnlyCsvRejection();
    allPassed &= testBinaryStlDetection();
    allPassed &= testOversizedFileLimit();

    std::cout << "========================================================\n";
    if (allPassed) {
        std::cout << "   ALL TASK B GEOMETRY & CSV UNIT TESTS PASSED (13/13)  \n";
        std::cout << "========================================================\n";
        return 0;
    } else {
        std::cerr << "   SOME TESTS FAILED!                                  \n";
        std::cout << "========================================================\n";
        return 1;
    }
}

