# osodb — the OSOLogic in-memory real-time cache (dependency-free core)
#
# The library itself needs only the C++17 standard library. Adapters that talk
# to external systems (MariaDB, gateways) are built separately under adapters/.
#
# © 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)
project(osodb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(osodb STATIC src/osodb.cpp)
target_include_directories(osodb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(osodb PUBLIC Threads::Threads)
target_compile_options(osodb PRIVATE -Wall -Wextra)

# Unit test (standalone, no test framework needed).
enable_testing()
add_executable(test_osodb tests/test_osodb.cpp)
target_link_libraries(test_osodb PRIVATE osodb)
add_test(NAME osodb COMMAND test_osodb)

# SQLite backend (embedded source of truth; the MCU foundation). Built only when
# libsqlite3 is available — the core library stays dependency-free.
find_library(SQLITE3_LIB sqlite3)
find_path(SQLITE3_INCLUDE sqlite3.h)
if(SQLITE3_LIB AND SQLITE3_INCLUDE)
  # The SQLite backend, plus the MCU engine (embedded SQLite store + MariaDB emulation).
  add_library(osodb_sqlite STATIC
    adapters/sql_backend.cpp adapters/sqlite_adapter.cpp
    adapters/mcu/mcudb.cpp adapters/mcu/mcustore.cpp)
  target_include_directories(osodb_sqlite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${SQLITE3_INCLUDE})
  target_link_libraries(osodb_sqlite PUBLIC osodb ${SQLITE3_LIB})
  target_compile_options(osodb_sqlite PRIVATE -Wall -Wextra)

  add_executable(test_sql_backend tests/test_sql_backend.cpp)
  target_link_libraries(test_sql_backend PRIVATE osodb_sqlite)
  add_test(NAME osodb_sql_backend COMMAND test_sql_backend)

  add_executable(test_mcudb tests/test_mcudb.cpp)
  target_link_libraries(test_mcudb PRIVATE osodb_sqlite)
  add_test(NAME osodb_mcudb COMMAND test_mcudb)

  add_executable(test_mcustore tests/test_mcustore.cpp)
  target_link_libraries(test_mcustore PRIVATE osodb_sqlite)
  add_test(NAME osodb_mcustore COMMAND test_mcustore)
else()
  message(STATUS "libsqlite3 not found — skipping osodb_sqlite backend and its tests")
endif()

# PostgreSQL backend (native libpq). Built only when the client dev files are present.
find_library(PQ_LIB pq)
find_path(PQ_INCLUDE libpq-fe.h PATH_SUFFIXES postgresql)
if(PQ_LIB AND PQ_INCLUDE)
  add_library(osodb_postgres STATIC adapters/sql_backend.cpp adapters/postgres_adapter.cpp)
  target_include_directories(osodb_postgres PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PQ_INCLUDE})
  target_link_libraries(osodb_postgres PUBLIC osodb ${PQ_LIB})
  target_compile_options(osodb_postgres PRIVATE -Wall -Wextra)
else()
  message(STATUS "libpq (libpq-fe.h) not found — skipping osodb_postgres backend")
endif()
