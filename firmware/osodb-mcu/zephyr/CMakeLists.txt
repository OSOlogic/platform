# Zephyr application build for the OSOlogic MCU tier.
#   west build -b rpi_pico   firmware/osodb-mcu/zephyr
#   west build -b esp32_devkitc_wroom/esp32/procpu firmware/osodb-mcu/zephyr
# Regenerate ../generated/board_map.c for your board first: make gen BOARD=<name>.
# (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(osodb_mcu)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/osodb_embedded.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/pid.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/federation.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/scan.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../generated/board_map.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/hal_zephyr.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_SOURCE_DIR}/../generated)

# inject Zephyr mutex locking into the portable core
target_compile_definitions(app PRIVATE OSODB_PORT_HEADER="osodb_port_zephyr.h")
