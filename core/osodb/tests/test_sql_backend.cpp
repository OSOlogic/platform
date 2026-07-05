/**
 * @file test_sql_backend.cpp
 * @brief End-to-end test of SqlAdapter over the SQLite backend against a MemoryHub.
 *
 * Seeds a SQLite file with two tag definitions (one ReadWrite, one ReadOnly), then drives the
 * full cycle: attach (read-through), poll_control (set-points in, ACL enforced), write + flush
 * (values back), and reads the DB to confirm.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include <cstdio>
#include <cstdlib>
#include <string>

#include "../adapters/sql_backend.hpp"
#include "../adapters/sqlite_adapter.hpp"
#include "../include/osodb.hpp"

using namespace osodb;

static int failures = 0;
static void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}

int main() {
  const std::string db = "/tmp/osodb_sqltest.sqlite";
  std::remove(db.c_str());

  // Seed the store: schema + two definitions, both with a pending set-point.
  {
    auto seed = make_sqlite_conn();
    seed->open(db);
    seed->exec(
        "CREATE TABLE osodb_tags (module_id INTEGER, io_id INTEGER, label TEXT, data_type INTEGER,"
        " access INTEGER, scale REAL DEFAULT 1, \"offset\" REAL DEFAULT 0, units TEXT,"
        " purpose INTEGER DEFAULT 1, value_raw INTEGER DEFAULT 0, quality INTEGER DEFAULT 0,"
        " ts_ns INTEGER DEFAULT 0, required_raw INTEGER, has_required INTEGER DEFAULT 0,"
        " PRIMARY KEY (module_id, io_id))");
    // (1,10) ReadWrite Float, set-point 42 ; (1,11) ReadOnly Int32, set-point 99 (must be denied)
    seed->exec("INSERT INTO osodb_tags(module_id,io_id,label,data_type,access,required_raw,has_required)"
               " VALUES (1,10,'pump_speed',5,1,42,1)");
    seed->exec("INSERT INTO osodb_tags(module_id,io_id,label,data_type,access,required_raw,has_required)"
               " VALUES (1,11,'flow_meter',4,0,99,1)");
  }

  MemoryHub hub;
  SqlAdapter adapter(make_sqlite_conn(), sqlite_dialect(), db);

  check(adapter.attach(hub), "attach() opens the SQLite store and loads definitions");
  check(hub.size() == 2, "read-through defined 2 tags in the hub");
  TagMeta m;
  check(hub.meta(TagKey{1, 10}, m) && m.label == "pump_speed" && m.type == DataType::Float,
        "definition mapped (label + Float type)");

  size_t applied = adapter.poll_control();
  check(applied == 1, "poll_control applied exactly 1 set-point (the ReadWrite one)");
  uint64_t rq = 0;
  check(hub.take_required(TagKey{1, 10}, rq) && rq == 42, "ReadWrite tag got required_raw = 42");
  check(!hub.has_required(TagKey{1, 11}), "ReadOnly tag set-point was DENIED by ACL");

  // Scan writes a current value; flush mirrors it back to the store.
  hub.write_current(TagKey{1, 10}, 12345, Quality::Good, 1000);
  check(adapter.flush() == 1, "flush wrote 1 changed value back to the store");

  {
    auto verify = make_sqlite_conn();
    verify->open(db);
    auto rows = verify->query("SELECT value_raw FROM osodb_tags WHERE module_id=1 AND io_id=10");
    check(!rows.empty() && rows[0][0] == "12345", "store now holds value_raw = 12345");
    auto cleared = verify->query("SELECT has_required FROM osodb_tags WHERE module_id=1 AND io_id=10");
    check(!cleared.empty() && cleared[0][0] == "0", "applied set-point was cleared (has_required=0)");
  }

  adapter.detach();
  std::remove(db.c_str());
  std::printf(failures ? "\nFAILED (%d)\n" : "\nALL PASS\n", failures);
  return failures ? 1 : 0;
}
