/**
 * @file sql_backend.cpp
 * @brief SqlAdapter — the shared IAdapter logic over any ISqlConn + SqlDialect.
 *
 * Portable schema (one table, works on SQLite / PostgreSQL / the MCU engine):
 *
 *   osodb_tags(module_id, io_id, label, data_type, access, scale, offset, units, purpose,
 *              value_raw, quality, ts_ns, required_raw, has_required, PRIMARY KEY(module_id, io_id))
 *
 * This is the CE store — a single denormalized table, not the PRO `devices/model_io_definition`
 * schema the MariaDB adapter wraps. It carries exactly what osodb needs: definitions, the current
 * mirror, and the `required_raw` control surface.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include "sql_backend.hpp"

#include <cstdlib>
#include <set>
#include <sstream>

namespace osodb {

const char* SqlDialect::name() const {
  switch (engine) {
    case Engine::MariaDB: return "MariaDB";
    case Engine::Postgres: return "PostgreSQL";
    case Engine::Sqlite: return "SQLite";
    case Engine::McuEmulated: return "MCU";
  }
  return "?";
}

struct SqlAdapter::Impl {
  std::unique_ptr<ISqlConn> conn;
  SqlDialect dialect;
  std::string dsn;
  FlushPolicy policy;
  IHub* hub = nullptr;
  int sub = 0;
  std::set<TagKey> dirty;  // tags whose current value changed since the last flush

  std::string ensure_schema() const {
    // Portable DDL: INTEGER/REAL/TEXT are common to SQLite, Postgres and the MCU engine.
    return "CREATE TABLE IF NOT EXISTS osodb_tags ("
           "module_id INTEGER NOT NULL, io_id INTEGER NOT NULL, label TEXT, "
           "data_type INTEGER NOT NULL, access INTEGER NOT NULL, scale REAL DEFAULT 1, "
           "\"offset\" REAL DEFAULT 0, units TEXT, purpose INTEGER DEFAULT 1, "
           "value_raw INTEGER DEFAULT 0, quality INTEGER DEFAULT 0, ts_ns INTEGER DEFAULT 0, "
           "required_raw INTEGER, has_required INTEGER DEFAULT 0, "
           "PRIMARY KEY (module_id, io_id))";
  }
};

SqlAdapter::SqlAdapter(std::unique_ptr<ISqlConn> conn, SqlDialect dialect, std::string dsn,
                       FlushPolicy policy)
    : impl_(new Impl{std::move(conn), dialect, std::move(dsn), policy, nullptr, 0, {}}) {}

SqlAdapter::~SqlAdapter() { detach(); }

const SqlDialect& SqlAdapter::dialect() const { return impl_->dialect; }

bool SqlAdapter::attach(IHub& hub) {
  impl_->hub = &hub;
  if (!impl_->conn->open(impl_->dsn)) return false;
  if (!impl_->conn->exec(impl_->ensure_schema())) return false;

  // Read-through: load every definition into the hub.
  auto rows = impl_->conn->query(
      "SELECT module_id, io_id, label, data_type, access, scale, \"offset\", units, purpose "
      "FROM osodb_tags");
  for (const auto& r : rows) {
    if (r.size() < 9) continue;
    TagKey key{static_cast<uint32_t>(std::strtoul(r[0].c_str(), nullptr, 10)),
               static_cast<uint32_t>(std::strtoul(r[1].c_str(), nullptr, 10))};
    TagMeta m;
    m.label = r[2];
    m.type = static_cast<DataType>(std::strtol(r[3].c_str(), nullptr, 10));
    m.access = static_cast<Access>(std::strtol(r[4].c_str(), nullptr, 10));
    m.scale = std::strtod(r[5].c_str(), nullptr);
    m.offset = std::strtod(r[6].c_str(), nullptr);
    m.units = r[7];
    m.purpose = static_cast<uint8_t>(std::strtol(r[8].c_str(), nullptr, 10));
    hub.define(key, m);
  }

  // Write-through: remember which tags change so flush() only writes those.
  impl_->sub = hub.subscribe([this](Event ev, const TagKey& key) {
    if (ev == Event::Current) impl_->dirty.insert(key);
  });
  return true;
}

size_t SqlAdapter::poll_control() {
  if (!impl_->hub) return 0;
  auto rows = impl_->conn->query(
      "SELECT module_id, io_id, required_raw FROM osodb_tags WHERE has_required = 1");
  size_t applied = 0;
  for (const auto& r : rows) {
    if (r.size() < 3) continue;
    TagKey key{static_cast<uint32_t>(std::strtoul(r[0].c_str(), nullptr, 10)),
               static_cast<uint32_t>(std::strtoul(r[1].c_str(), nullptr, 10))};
    // Enforce access: never apply a set-point to a read-only tag.
    TagMeta m;
    if (impl_->hub->meta(key, m) && m.access == Access::ReadOnly) continue;
    impl_->hub->set_required(key, std::strtoull(r[2].c_str(), nullptr, 10));
    ++applied;
  }
  if (applied) impl_->conn->exec("UPDATE osodb_tags SET has_required = 0 WHERE has_required = 1");
  return applied;
}

size_t SqlAdapter::flush() {
  if (!impl_->hub) return 0;
  size_t n = 0;
  for (const auto& key : impl_->dirty) {
    Sample s;
    if (!impl_->hub->read(key, s)) continue;
    std::ostringstream q;
    q << "UPDATE osodb_tags SET value_raw = " << s.raw
      << ", quality = " << static_cast<int>(s.quality) << ", ts_ns = " << s.ts_ns
      << " WHERE module_id = " << key.module_id << " AND io_id = " << key.io_definition_id;
    if (impl_->conn->exec(q.str())) ++n;
  }
  impl_->dirty.clear();
  return n;
}

void SqlAdapter::detach() {
  if (impl_->hub && impl_->sub) {
    impl_->hub->unsubscribe(impl_->sub);
    impl_->sub = 0;
  }
  if (impl_->conn) impl_->conn->close();
  impl_->hub = nullptr;
}

}  // namespace osodb
