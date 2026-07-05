/**
 * @file sqlite_adapter.cpp
 * @brief ISqlConn over libsqlite3 + the SQLite dialect. Drives the shared SqlAdapter.
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include "sqlite_adapter.hpp"

#include <sqlite3.h>

namespace osodb {
namespace {

class SqliteConn final : public ISqlConn {
 public:
  ~SqliteConn() override { close(); }

  bool open(const std::string& dsn) override {
    close();
    return sqlite3_open(dsn.empty() ? ":memory:" : dsn.c_str(), &db_) == SQLITE_OK;
  }

  bool exec(const std::string& sql) override {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (err) {
      err_ = err;
      sqlite3_free(err);
    }
    return rc == SQLITE_OK;
  }

  std::vector<SqlRow> query(const std::string& sql) override {
    std::vector<SqlRow> rows;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
      err_ = sqlite3_errmsg(db_);
      return rows;
    }
    const int cols = sqlite3_column_count(st);
    while (sqlite3_step(st) == SQLITE_ROW) {
      SqlRow row;
      row.reserve(static_cast<size_t>(cols));
      for (int i = 0; i < cols; ++i) {
        const unsigned char* text = sqlite3_column_text(st, i);
        row.emplace_back(text ? reinterpret_cast<const char*>(text) : "");
      }
      rows.push_back(std::move(row));
    }
    sqlite3_finalize(st);
    return rows;
  }

  std::string last_error() const override { return err_; }

  void close() override {
    if (db_) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
  }

 private:
  sqlite3* db_ = nullptr;
  std::string err_;
};

}  // namespace

std::unique_ptr<ISqlConn> make_sqlite_conn() { return std::make_unique<SqliteConn>(); }

SqlDialect sqlite_dialect() {
  SqlDialect d;
  d.engine = SqlDialect::Engine::Sqlite;
  d.id_quote = '"';
  d.supports_upsert = true;
  d.upsert_clause = "ON CONFLICT(module_id, io_id) DO UPDATE SET";
  d.required_value_control = true;
  return d;
}

}  // namespace osodb
