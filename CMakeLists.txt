cmake_minimum_required(VERSION 3.16)
project(UnifiedEngineeringWorkbench VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Link MSVC runtime statically (/MT in Release, /MTd in Debug) so binaries run on clean Windows 11 without redistributable DLLs
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Optimization and compiler options
if(MSVC)
    add_compile_options(/W4 /EHsc /utf-8 /permissive-)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
        add_compile_options(/O2 /Oi /Ot /Gy)
    endif()
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -pedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
        add_compile_options(-O3 -march=native)
    endif()
endif()

# Global include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Core sources
set(CORE_SOURCES
    src/core/Logger.cpp
    src/core/SystemInfo.cpp
    src/server/HttpServer.cpp
    src/geometry/StlParser.cpp
    src/geometry/CsvParser.cpp
    src/thermal/ThermalSolver1D.cpp
    src/performance/ThreadPool.cpp
    src/performance/PerformanceBenchmark.cpp
    src/hardware/McuDeviceAdapter.cpp
    src/hardware/FpgaDeviceAdapter.cpp
    src/hardware/JtagAdapter.cpp
    src/hardware/HardwareManager.cpp
    src/neural/NeuralNetRegression.cpp
)

# Main server executable
add_executable(workbench_server
    src/main.cpp
    ${CORE_SOURCES}
)

if(WIN32)
    target_link_libraries(workbench_server PRIVATE ws2_32 crypt32)
endif()

# Standalone Windows Launcher executable (Launch.exe)
if(WIN32)
    add_executable(Launch WIN32
        src/launcher/launch.cpp
    )
    target_link_libraries(Launch PRIVATE ws2_32 shell32 user32)
    # Output Launch.exe directly into project root or bin
    set_target_properties(Launch PROPERTIES
        OUTPUT_NAME "Launch"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
    set_target_properties(workbench_server PROPERTIES
        OUTPUT_NAME "workbench_server"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )

    # Post-build step: Copy Launch.exe and workbench_server.exe to root directory for one-click testing
    add_custom_command(TARGET Launch POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Launch>
            "${CMAKE_CURRENT_SOURCE_DIR}/Launch.exe"
        COMMENT "Deploying Launch.exe to project root"
    )
    add_custom_command(TARGET workbench_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:workbench_server>
            "${CMAKE_CURRENT_SOURCE_DIR}/workbench_server.exe"
        COMMENT "Deploying workbench_server.exe to project root"
    )
endif()

# Testing framework setup
enable_testing()

add_executable(test_geometry
    tests/test_geometry.cpp
    src/geometry/StlParser.cpp
    src/geometry/CsvParser.cpp
)
add_test(NAME GeometryAndCsvTests COMMAND test_geometry)

add_executable(test_thermal
    tests/test_thermal.cpp
    src/thermal/ThermalSolver1D.cpp
)
add_test(NAME ThermalFEMTests COMMAND test_thermal)

add_executable(test_performance
    tests/test_performance.cpp
    src/performance/ThreadPool.cpp
    src/performance/PerformanceBenchmark.cpp
    src/thermal/ThermalSolver1D.cpp
)
add_test(NAME PerformanceBenchmarkTests COMMAND test_performance)

add_executable(test_hardware
    tests/test_hardware.cpp
    src/hardware/McuDeviceAdapter.cpp
    src/hardware/FpgaDeviceAdapter.cpp
    src/hardware/JtagAdapter.cpp
    src/hardware/HardwareManager.cpp
)
add_test(NAME HardwareIntegrationTests COMMAND test_hardware)

add_executable(test_neural
    tests/test_neural.cpp
    src/neural/NeuralNetRegression.cpp
)
add_test(NAME NeuralBackpropTests COMMAND test_neural)

