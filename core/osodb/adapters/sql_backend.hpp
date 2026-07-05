/**
 * @file sql_backend.hpp
 * @brief SqlAdapter — one IAdapter that works over any SQL backend (MariaDB, PostgreSQL, SQLite).
 *
 * MariaDB, PostgreSQL and SQLite differ only in their *driver* and a handful of
 * *dialect* details (identifier quoting, the upsert clause, placeholder style). The
 * work an adapter does — read tag definitions in at attach(), pull `required_value`
 * set-points in poll_control(), write current values back in flush() — is identical.
 *
 * So this header factors that seam in two:
 *   - `ISqlConn`  — the thin driver surface a backend must provide (exec / query).
 *   - `SqlDialect`— the small per-engine differences, as data.
 *   - `SqlAdapter`— the shared IAdapter logic, written once against `ISqlConn`.
 *
 * A concrete backend (postgres_adapter, sqlite_adapter, the MariaDB one) only supplies
 * an `ISqlConn` and a `SqlDialect`; it does not re-implement the reconciliation logic.
 * This header is dependency-free; a driver's client library is included in its own .cpp.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "adapter.hpp"

namespace osodb {

/// One result row as strings (the adapter parses types against the tag definitions).
using SqlRow = std::vector<std::string>;

/// The minimal driver surface a SQL backend must provide. Concrete drivers (libpq,
/// sqlite3, the MariaDB client) implement this in their .cpp.
class ISqlConn {
 public:
  virtual ~ISqlConn() = default;
  /// Open the connection from a DSN/URI/path. Returns true on success.
  virtual bool open(const std::string& dsn) = 0;
  /// Run a statement with no result set (DDL/DML). Returns true on success.
  virtual bool exec(const std::string& sql) = 0;
  /// Run a query and return its rows. Empty on error (see last_error()).
  virtual std::vector<SqlRow> query(const std::string& sql) = 0;
  /// Human-readable text of the last failure.
  virtual std::string last_error() const = 0;
  virtual void close() = 0;
};

/// The per-engine differences, expressed as data rather than code branches.
struct SqlDialect {
  enum class Engine { MariaDB, Postgres, Sqlite, McuEmulated };
  Engine engine = Engine::Sqlite;
  char id_quote = '`';                    ///< identifier quote: ` for MariaDB, " for Postgres/SQLite
  bool supports_upsert = true;            ///< ON CONFLICT / ON DUPLICATE KEY available
  std::string upsert_clause;              ///< engine-specific upsert tail (filled by the backend)
  bool required_value_control = true;     ///< SQL-authored set-points are a control surface here
  const char* name() const;               ///< "MariaDB" / "PostgreSQL" / "SQLite" / "MCU"
};

/**
 * The shared IAdapter, written once against ISqlConn + SqlDialect.
 *
 * attach():        SELECT the tag definitions and define() them in the hub, then wire
 *                  write-through of current values.
 * poll_control():  SELECT the externally-authored required_value rows and set_required().
 * flush():         batch-write buffered current values back (upsert into the mirror table).
 *
 * Backends construct this with their own connection + dialect via the factory.
 */
class SqlAdapter final : public IAdapter {
 public:
  SqlAdapter(std::unique_ptr<ISqlConn> conn, SqlDialect dialect, std::string dsn,
             FlushPolicy policy = FlushPolicy::WriteBack);
  ~SqlAdapter() override;

  bool attach(IHub& hub) override;
  size_t poll_control() override;
  size_t flush() override;
  void detach() override;

  const SqlDialect& dialect() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace osodb
