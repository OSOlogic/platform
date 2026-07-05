/**
 * @file factory.hpp
 * @brief make_adapter — pick a persistence backend for osodb at run time.
 *
 * osodb is the cache; an adapter is the source of truth behind it. This factory hides which
 * one you built with, so the core just asks for a backend by name/DSN:
 *
 *   auto adapter = osodb::make_adapter({osodb::Backend::Postgres,
 *                                       "postgresql://user@host/osodb"});
 *   adapter->attach(hub);
 *
 * Postgres / SQLite / MCU are assembled as a `SqlAdapter` over the matching `ISqlConn` and
 * `SqlDialect`; MariaDB keeps its dedicated adapter. Because osodb owns the real-time path,
 * the choice of engine is a deployment decision, not a performance one.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#pragma once

#include <memory>
#include <string>

#include "adapter.hpp"

namespace osodb {

/// The persistence backends osodb can sit in front of.
enum class Backend { MariaDB, Postgres, Sqlite, McuEmulated };

struct AdapterSpec {
  Backend backend = Backend::Sqlite;
  std::string dsn;                        ///< conninfo / URI / file path (":memory:" for embedded)
  FlushPolicy policy = FlushPolicy::WriteBack;
};

/// Build the adapter for @p spec. Returns nullptr if this build lacks that driver.
std::unique_ptr<IAdapter> make_adapter(const AdapterSpec& spec);

/// Parse a backend name: "mariadb" | "postgres"/"postgresql" | "sqlite" | "mcu". Defaults to Sqlite.
Backend backend_from_name(const std::string& name);

}  // namespace osodb
