/**
 * @file postgres_adapter.cpp
 * @brief ISqlConn over libpq (native) + the PostgreSQL dialect. Drives the shared SqlAdapter.
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include "postgres_adapter.hpp"

#include <libpq-fe.h>

namespace osodb {
namespace {

class PostgresConn final : public ISqlConn {
 public:
  ~PostgresConn() override { close(); }

  bool open(const std::string& dsn) override {
    close();
    conn_ = PQconnectdb(dsn.c_str());
    if (PQstatus(conn_) != CONNECTION_OK) {
      err_ = PQerrorMessage(conn_);
      close();
      return false;
    }
    return true;
  }

  bool exec(const std::string& sql) override {
    PGresult* res = PQexec(conn_, sql.c_str());
    const ExecStatusType st = PQresultStatus(res);
    const bool ok = (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK);
    if (!ok) err_ = PQerrorMessage(conn_);
    PQclear(res);
    return ok;
  }

  std::vector<SqlRow> query(const std::string& sql) override {
    std::vector<SqlRow> rows;
    PGresult* res = PQexec(conn_, sql.c_str());
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
      const int nr = PQntuples(res), nc = PQnfields(res);
      for (int i = 0; i < nr; ++i) {
        SqlRow row;
        row.reserve(static_cast<size_t>(nc));
        for (int j = 0; j < nc; ++j) {
          row.emplace_back(PQgetisnull(res, i, j) ? "" : PQgetvalue(res, i, j));
        }
        rows.push_back(std::move(row));
      }
    } else {
      err_ = PQerrorMessage(conn_);
    }
    PQclear(res);
    return rows;
  }

  std::string last_error() const override { return err_; }

  void close() override {
    if (conn_) {
      PQfinish(conn_);
      conn_ = nullptr;
    }
  }

 private:
  PGconn* conn_ = nullptr;
  std::string err_;
};

}  // namespace

std::unique_ptr<ISqlConn> make_postgres_conn() { return std::make_unique<PostgresConn>(); }

SqlDialect postgres_dialect() {
  SqlDialect d;
  d.engine = SqlDialect::Engine::Postgres;
  d.id_quote = '"';
  d.supports_upsert = true;
  d.upsert_clause = "ON CONFLICT (module_id, io_id) DO UPDATE SET";
  d.required_value_control = true;
  return d;
}

}  // namespace osodb
