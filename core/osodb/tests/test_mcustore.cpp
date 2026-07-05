/**
 * @file test_mcustore.cpp
 * @brief The bare-metal MCU store runs the osodb SQL subset and drives the SqlAdapter — no
 *        filesystem, no allocation per operation.
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include <cstdio>
#include <string>

#include "../adapters/mcu/mcudb.hpp"
#include "../adapters/mcu/mcustore.hpp"
#include "../adapters/sql_backend.hpp"
#include "../include/osodb.hpp"

using namespace osodb;

static int failures = 0;
static void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}

static void seed(ISqlConn& c) {
  c.exec("CREATE TABLE osodb_tags (module_id INTEGER, io_id INTEGER, label TEXT, data_type INTEGER,"
         " access INTEGER, required_raw INTEGER, has_required INTEGER)");
  c.exec("INSERT INTO osodb_tags(module_id,io_id,label,data_type,access,required_raw,has_required)"
         " VALUES (1,10,'pump_speed',5,1,42,1)");
  c.exec("INSERT INTO osodb_tags(module_id,io_id,label,data_type,access,required_raw,has_required)"
         " VALUES (1,11,'flow_meter',4,0,99,1)");
}

int main() {
  // Part A — the store's SQL subset, verified directly on one connection.
  {
    auto c = make_mcu_bare_conn();
    c->open("");
    seed(*c);
    check(c->query("SELECT module_id, io_id, label, data_type, access, scale, offset, units, purpose "
                   "FROM osodb_tags").size() == 2, "definitions SELECT returns 2 rows");
    check(c->query("SELECT * FROM osodb_tags WHERE has_required = 1").size() == 2,
          "required-value SELECT returns 2 pending set-points");
    c->exec("UPDATE osodb_tags SET value_raw = 777, quality = 0, ts_ns = 5 WHERE module_id = 1 AND io_id = 10");
    auto v = c->query("SELECT value_raw FROM osodb_tags WHERE module_id = 1 AND io_id = 10");
    check(!v.empty() && v[0][0] == "777", "UPDATE value_raw then SELECT reads it back (777)");
  }

  // Part B — the shared SqlAdapter runs on the bare store, ACL enforced.
  {
    auto c = make_mcu_bare_conn();
    c->open("");
    seed(*c);
    MemoryHub hub;
    SqlAdapter adapter(std::move(c), mcu_dialect(), "");
    check(adapter.attach(hub), "SqlAdapter attaches to the bare store");
    check(hub.size() == 2, "read-through defined 2 tags");
    check(adapter.poll_control() == 1, "poll_control applied 1 set-point (ReadWrite only)");
    uint64_t rq = 0;
    check(hub.take_required(TagKey{1, 10}, rq) && rq == 42, "ReadWrite tag got required_raw = 42");
    check(!hub.has_required(TagKey{1, 11}), "ReadOnly tag set-point DENIED by ACL");
    hub.write_current(TagKey{1, 10}, 2024, Quality::Good, 9);
    check(adapter.flush() == 1, "flush wrote 1 value back to the bare store");
    adapter.detach();
  }

  std::printf(failures ? "\nFAILED (%d)\n" : "\nALL PASS\n", failures);
  return failures ? 1 : 0;
}
