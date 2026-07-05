/**
 * @file test_mcudb.cpp
 * @brief The MCU engine runs MariaDB-dialect SQL on its embedded (SQLite) store.
 *
 * Proves the emulation layer: backtick identifiers, ENGINE=… table options, and
 * `INSERT … ON DUPLICATE KEY UPDATE` upserts all execute unchanged on the MCU engine.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include <cstdio>
#include <string>

#include "../adapters/mcu/mcudb.hpp"

using namespace osodb;

static int failures = 0;
static void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}

int main() {
  auto conn = make_mcu_conn();
  check(conn->open(":memory:"), "MCU engine opens (embedded SQLite store)");

  // MariaDB dialect: backtick identifiers + a server storage engine option.
  check(conn->exec("CREATE TABLE `tags` (`k` INTEGER PRIMARY KEY, `v` INTEGER) ENGINE=MEMORY"),
        "MariaDB CREATE (backticks + ENGINE=MEMORY) runs unchanged");
  check(conn->exec("INSERT INTO `tags` (`k`,`v`) VALUES (1, 100)"), "insert a row");

  // MariaDB upsert emulated on the embedded store.
  check(conn->exec("INSERT INTO `tags` (`k`,`v`) VALUES (1, 200) ON DUPLICATE KEY UPDATE `v` = 200"),
        "INSERT … ON DUPLICATE KEY UPDATE is accepted");
  auto rows = conn->query("SELECT `v` FROM `tags` WHERE `k` = 1");
  check(!rows.empty() && rows[0][0] == "200", "upsert updated the row (100 -> 200)");

  check(mcu_dialect().engine == SqlDialect::Engine::McuEmulated, "dialect reports McuEmulated");
  check(std::string(mcu_dialect().name()) == "MCU", "dialect name is MCU");

  std::printf(failures ? "\nFAILED (%d)\n" : "\nALL PASS\n", failures);
  return failures ? 1 : 0;
}
