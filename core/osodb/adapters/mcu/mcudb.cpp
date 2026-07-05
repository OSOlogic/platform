/**
 * @file mcudb.cpp
 * @brief McuDb — SQLite-backed embedded store with a MariaDB emulation layer.
 *
 * On targets with a filesystem the storage IS SQLite (reused via make_sqlite_conn), so the MCU
 * path shares the tested driver. On top of it a small translator rewrites the MariaDB-isms the
 * legacy store uses into the SQLite the engine runs — so MariaDB-targeted SQL executes unchanged:
 *
 *   `` ` ``-quoted identifiers        → "-quoted (SQLite accepts both, normalized for clarity)
 *   ENGINE=… / AUTO_INCREMENT / …     → stripped (no server storage engines here)
 *   INSERT … ON DUPLICATE KEY UPDATE  → INSERT OR REPLACE (upsert by primary key)
 *
 * This is the "compatible, not complete" subset: it covers what osodb issues, not all of MySQL.
 * The fixed-size, allocation-free built-in store for bare-metal targets (no filesystem) is a
 * future storage backend behind the same translator; McuDbLimits reserves its bounds.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include "mcudb.hpp"

#include <regex>
#include <string>

#include "../sqlite_adapter.hpp"

namespace osodb {
namespace {

/// Rewrite the MariaDB subset the store uses into SQLite. Compatible, not complete.
std::string translate_mariadb(std::string sql) {
  // Backtick identifiers → double-quoted.
  for (char& c : sql) {
    if (c == '`') c = '"';
  }
  // Drop server-only table options.
  sql = std::regex_replace(sql, std::regex(R"(\s+ENGINE\s*=\s*\w+)", std::regex::icase), "");
  sql = std::regex_replace(sql, std::regex(R"(\s+AUTO_INCREMENT(\s*=\s*\d+)?)", std::regex::icase), "");
  sql = std::regex_replace(sql, std::regex(R"(\s+DEFAULT\s+CHARSET\s*=\s*\w+)", std::regex::icase), "");
  // INSERT … ON DUPLICATE KEY UPDATE … → upsert by primary key.
  std::smatch m;
  if (std::regex_search(sql, m, std::regex(R"(ON\s+DUPLICATE\s+KEY\s+UPDATE)", std::regex::icase))) {
    sql = sql.substr(0, static_cast<size_t>(m.position(0)));  // cut the UPDATE tail
    sql = std::regex_replace(sql, std::regex(R"(^\s*INSERT\s+INTO)", std::regex::icase),
                             "INSERT OR REPLACE INTO");
  }
  return sql;
}

/// ISqlConn over SQLite, with MariaDB SQL translated on the way in.
class McuConn final : public ISqlConn {
 public:
  explicit McuConn(McuDbLimits limits) : limits_(limits) {}

  bool open(const std::string& dsn) override { return inner_->open(dsn); }
  bool exec(const std::string& sql) override { return inner_->exec(translate_mariadb(sql)); }
  std::vector<SqlRow> query(const std::string& sql) override {
    return inner_->query(translate_mariadb(sql));
  }
  std::string last_error() const override { return inner_->last_error(); }
  void close() override { inner_->close(); }

 private:
  McuDbLimits limits_;
  std::unique_ptr<ISqlConn> inner_ = make_sqlite_conn();
};

}  // namespace

std::unique_ptr<ISqlConn> make_mcu_conn(McuDbLimits limits) {
  return std::make_unique<McuConn>(limits);
}

SqlDialect mcu_dialect() {
  SqlDialect d;
  d.engine = SqlDialect::Engine::McuEmulated;
  d.id_quote = '`';                  // presents as MariaDB so callers write MariaDB SQL
  d.supports_upsert = true;
  d.upsert_clause = "ON DUPLICATE KEY UPDATE";
  d.required_value_control = true;
  return d;
}

}  // namespace osodb
