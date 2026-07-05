/**
 * @file sqlite_adapter.hpp
 * @brief SQLite backend for osodb — an embedded, serverless source of truth.
 *
 * SQLite needs no server: the whole store is one file (or `:memory:`). That makes it the
 * natural choice for a single-box gateway, a bench setup, or CI — and, crucially, it is the
 * **foundation of the MCU story**: on a microcontroller SQLite (or a SQLite-subset build) is
 * the embedded engine, under a MariaDB *emulation* layer so MariaDB-targeted code runs
 * unchanged (see mcu/mcudb.hpp). As with Postgres, the compatibility point holds: osodb is the
 * in-memory cache, so SQLite only has to persist — no MEMORY-table equivalent is needed.
 *
 * It plugs into the shared `SqlAdapter` by providing an `ISqlConn` over libsqlite3 and
 * the SQLite dialect. The reconciliation logic (definitions in, set-points in, values
 * back) is not re-implemented here — it lives once in sql_backend.
 *
 *   auto conn = osodb::make_sqlite_conn();
 *   osodb::SqlAdapter adapter(std::move(conn), osodb::sqlite_dialect(),
 *                             "/var/lib/osologic/osodb.sqlite");
 *
 * The .cpp includes <sqlite3.h>; this header is dependency-free.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#pragma once

#include <memory>

#include "sql_backend.hpp"

namespace osodb {

/// A connection over libsqlite3. DSN is a file path or ":memory:".
std::unique_ptr<ISqlConn> make_sqlite_conn();

/// SQLite dialect: "-quoted identifiers, INSERT … ON CONFLICT DO UPDATE upsert.
SqlDialect sqlite_dialect();

}  // namespace osodb
