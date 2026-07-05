/**
 * @file postgres_adapter.hpp
 * @brief PostgreSQL backend for osodb — native libpq, via the shared SQL compatibility layer.
 *
 * PostgreSQL connects natively (libpq); what it needs is the *compatibility layer*, because
 * the legacy store leans on MariaDB specifics:
 *
 *   - **MariaDB MEMORY-engine mirror.** MariaDB keeps the real-time mirror (`rtmirror`) in a
 *     MEMORY table for speed. Postgres has no MEMORY engine. **This is why it isn't critical:**
 *     with osodb as the in-memory cache, the *hot path never touches the DB* — the mirror is
 *     just persistence and the SQL control surface. So on Postgres the mirror is a plain
 *     (optionally `UNLOGGED`) table; the real-time performance comes from osodb, not the engine.
 *   - **Dialect.** "-quoted identifiers and `INSERT … ON CONFLICT (…) DO UPDATE` upsert
 *     (vs MariaDB's `` ` ``-quotes and `ON DUPLICATE KEY UPDATE`). Captured as data in
 *     `postgres_dialect()`, so the shared `SqlAdapter` needs no engine branches.
 *
 * Using Postgres as the *default* store in place of MariaDB is therefore feasible precisely
 * because osodb decouples runtime latency from the database engine.
 *
 *   auto conn = osodb::make_postgres_conn();
 *   osodb::SqlAdapter adapter(std::move(conn), osodb::postgres_dialect(),
 *                             "postgresql://user@host/osodb");
 *
 * The .cpp includes <libpq-fe.h>; this header is dependency-free.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#pragma once

#include <memory>

#include "sql_backend.hpp"

namespace osodb {

/// A native connection over libpq. DSN is a libpq conninfo string or postgresql:// URI.
std::unique_ptr<ISqlConn> make_postgres_conn();

/// PostgreSQL dialect: "-quoted identifiers, ON CONFLICT DO UPDATE upsert, plain mirror table.
SqlDialect postgres_dialect();

}  // namespace osodb
