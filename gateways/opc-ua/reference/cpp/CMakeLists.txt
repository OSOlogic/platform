# OSOLogic OPC-UA gateway — native C++ reference (open62541)
#
# Needs: open62541 (https://open62541.org) and the osodb library (core/osodb).
#
#   sudo apt install libopen62541-dev     # or build open62541 from source
#   cmake -S . -B build && cmake --build build
#   ./build/osologic_opcua_server         # opc.tcp://0.0.0.0:4840
#
# © 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)
project(osologic_opcua_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(open62541 REQUIRED)

# Build the dependency-free osodb hub library from the core.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../core/osodb osodb-build)

add_executable(osologic_opcua_server opcua_server.cpp)
target_link_libraries(osologic_opcua_server PRIVATE open62541::open62541 osodb)
target_compile_options(osologic_opcua_server PRIVATE -Wall -Wextra)
