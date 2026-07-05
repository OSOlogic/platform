/**
 * @file mcudb.hpp
 * @brief McuDb — an embedded, MariaDB-emulating store for microcontrollers.
 *
 * On a microcontroller there is no MariaDB server and often no filesystem, yet the same
 * program should run: read tag definitions, expose `required_value` set-points, mirror
 * current values. McuDb provides that as a tiny embedded engine with a **MariaDB emulation
 * layer**, so code and SQL written for the MariaDB store run **unchanged** on the MCU.
 *
 * How the pieces fit:
 *   - **Storage.** Where a filesystem exists, McuDb is backed by SQLite (see sqlite_adapter);
 *     on the smallest targets it uses a fixed-size, allocation-free built-in table store.
 *   - **MariaDB emulation.** It accepts the MariaDB dialect the store uses — `` ` ``-quoted
 *     identifiers, `INSERT … ON DUPLICATE KEY UPDATE`, and the handful of statements osodb
 *     issues against `tags` / `rtmirror` — and translates them to its storage. So the *same*
 *     `SqlAdapter` drives it: `make_mcu_conn()` returns an `ISqlConn`, `mcu_dialect()` presents
 *     as MariaDB-compatible.
 *   - **Why this is enough.** osodb is the in-memory cache and the real-time path; McuDb only
 *     has to hold definitions and the mirror. It does not need MariaDB's MEMORY engine, its
 *     network protocol, its query planner, or transactions across many tables.
 *
 * **Compatible, not complete (by design).** McuDb implements the *subset* osodb needs, not the
 * full SQL surface: no joins beyond the fixed schema, bounded row counts, integer/real/short-
 * string columns, single-writer. The same engine also builds on Linux as a dependency-free,
 * lightweight alternative to a real server — with the same subset limits, so what runs on the
 * MCU runs the same on the desktop.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#pragma once

#include <memory>

#include "../sql_backend.hpp"

namespace osodb {

/// Compile-time bounds for the allocation-free built-in store (tune per target).
struct McuDbLimits {
  int max_tags = 256;        ///< definitions + mirror rows
  int max_name = 32;         ///< identifier / short-string column width
};

/// An ISqlConn over the embedded McuDb engine (SQLite storage + MariaDB emulation), for MCU
/// targets that have a filesystem. The DSN is a file path or ":memory:".
/// For bare-metal targets with no filesystem, use `make_mcu_bare_conn()` (see mcustore.hpp) —
/// the fixed-size, allocation-free store. Both drive the same SqlAdapter.
std::unique_ptr<ISqlConn> make_mcu_conn(McuDbLimits limits = {});

/// The MCU dialect: presents as MariaDB-compatible (so MariaDB SQL runs) while the engine
/// underneath is the embedded store. `SqlDialect::Engine::McuEmulated`.
SqlDialect mcu_dialect();

}  // namespace osodb
