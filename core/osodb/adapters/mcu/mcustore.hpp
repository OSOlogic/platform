/**
 * @file mcustore.hpp
 * @brief McuStore — a fixed-size, allocation-free tag store for bare-metal MCUs.
 *
 * The MCU engine's storage where there is no filesystem: one static array of rows, no malloc,
 * no SQLite. It backs an `ISqlConn` that recognizes exactly the statement templates osodb issues
 * against `osodb_tags` (CREATE / INSERT / the two SELECTs / the two UPDATEs) — the "compatible,
 * not complete" subset. That's enough for the shared `SqlAdapter` to run unchanged on an RP2040 /
 * STM32 / ESP32, so the same code path works from a microcontroller up to a server.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <memory>

#include "../sql_backend.hpp"

namespace osodb {

/// One tag row, mirroring the portable osodb_tags schema (fixed-width, no heap).
struct McuRow {
  bool used = false;
  uint32_t module_id = 0;
  uint32_t io_id = 0;
  char label[32] = {0};
  int32_t data_type = 0;
  int32_t access = 0;
  double scale = 1.0;
  double offset = 0.0;
  uint64_t value_raw = 0;
  int32_t quality = 0;
  uint64_t ts_ns = 0;
  uint64_t required_raw = 0;
  bool has_required = false;
};

/// Fixed-capacity table store. `Cap` rows are reserved statically; no allocation ever happens.
template <int Cap = 256>
struct McuStore {
  McuRow rows[Cap];
  static constexpr int capacity = Cap;

  McuRow* find(uint32_t m, uint32_t io) {
    for (auto& r : rows) {
      if (r.used && r.module_id == m && r.io_id == io) return &r;
    }
    return nullptr;
  }
  McuRow* alloc() {
    for (auto& r : rows) {
      if (!r.used) return &r;
    }
    return nullptr;  // full — bounded by design
  }
};

/// Build an ISqlConn over a bare-metal store (default capacity 256 rows). Recognizes the osodb
/// statement subset; unrecognized SQL fails cleanly rather than pretending to run.
std::unique_ptr<ISqlConn> make_mcu_bare_conn(int capacity = 256);

}  // namespace osodb
