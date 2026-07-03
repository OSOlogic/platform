/**
 * @file test_osodb.cpp
 * @brief Standalone unit test for the osodb MemoryHub (no external deps).
 *
 * Build & run:
 *   g++ -std=c++17 -pthread core/osodb/src/osodb.cpp core/osodb/tests/test_osodb.cpp -o /tmp/t && /tmp/t
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "../include/osodb.hpp"

using namespace osodb;

static int g_checks = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    ++g_checks;                                                           \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      std::exit(1);                                                       \
    }                                                                     \
  } while (0)

static uint64_t float_bits(float f) {
  uint32_t b;
  std::memcpy(&b, &f, sizeof(b));
  return b;
}

static void test_tagkey_roundtrip() {
  TagKey k{7, 42};
  CHECK(k.to_string() == "7.42");
  auto p = TagKey::parse("7.42");
  CHECK(p.has_value());
  CHECK(*p == k);
  CHECK(!TagKey::parse("").has_value());
  CHECK(!TagKey::parse("7").has_value());
  CHECK(!TagKey::parse("7.").has_value());
  CHECK(!TagKey::parse(".42").has_value());
  CHECK(!TagKey::parse("7.x").has_value());
}

static void test_define_read_write() {
  MemoryHub hub;
  TagKey k{1, 10};
  CHECK(!hub.is_defined(k));
  TagMeta m;
  m.type = DataType::UInt16;
  m.access = Access::ReadWrite;
  m.label = "Conveyor_Speed";
  m.units = "rpm";
  hub.define(k, m);
  CHECK(hub.is_defined(k));
  CHECK(hub.size() == 1);

  Sample s;
  CHECK(hub.read(k, s));               // defined but not yet written
  CHECK(s.quality == Quality::BadStale);

  hub.write_current(k, 1450, Quality::Good);
  CHECK(hub.read(k, s));
  CHECK(s.raw == 1450);
  CHECK(s.quality == Quality::Good);
  CHECK(s.ts_ns > 0);

  TagMeta back;
  CHECK(hub.meta(k, back));
  CHECK(back.label == "Conveyor_Speed");
  CHECK(back.type == DataType::UInt16);
}

static void test_engineering_scaling() {
  TagMeta m;
  m.type = DataType::Int16;
  m.scale = 0.1;
  m.offset = -5.0;
  Sample s;
  s.raw = static_cast<uint64_t>(static_cast<uint16_t>(static_cast<int16_t>(-30)));
  // -30 * 0.1 + (-5) = -8.0
  CHECK(std::fabs(s.engineering(m) - (-8.0)) < 1e-9);

  TagMeta mf;
  mf.type = DataType::Float;
  mf.scale = 1.0;
  mf.offset = 0.0;
  Sample sf;
  sf.raw = float_bits(23.5f);
  CHECK(std::fabs(sf.engineering(mf) - 23.5) < 1e-6);

  TagMeta mb;
  mb.type = DataType::Boolean;
  Sample sb;
  sb.raw = 1;
  CHECK(std::fabs(sb.engineering(mb) - 1.0) < 1e-9);
}

static void test_required_setpoints() {
  MemoryHub hub;
  TagKey a{2, 1}, b{2, 2};
  hub.define(a, {});
  hub.define(b, {});

  CHECK(!hub.has_required(a));
  hub.set_required(a, 2500);          // e.g. SQL: UPDATE ... SET required_value=2500
  CHECK(hub.has_required(a));

  uint64_t v = 0;
  CHECK(hub.take_required(a, v));      // consumed by the scan bridge
  CHECK(v == 2500);
  CHECK(!hub.has_required(a));         // cleared after take
  CHECK(!hub.take_required(a, v));     // nothing pending now

  hub.set_required(a, 10);
  hub.set_required(b, 20);
  auto drained = hub.drain_required();
  CHECK(drained.size() == 2);
  CHECK(!hub.has_required(a) && !hub.has_required(b));
}

static void test_module_status() {
  MemoryHub hub;
  ModuleStatus s0 = hub.module_status(99);
  CHECK(!s0.connected);
  hub.set_module_status(99, true);
  ModuleStatus s1 = hub.module_status(99);
  CHECK(s1.connected);
  CHECK(s1.last_seen_ns > 0);
}

static void test_subscriptions() {
  MemoryHub hub;
  std::atomic<int> current_events{0}, required_events{0};
  int id = hub.subscribe([&](Event ev, const TagKey&) {
    if (ev == Event::Current) current_events++;
    if (ev == Event::Required) required_events++;
  });
  TagKey k{3, 3};
  hub.define(k, {});
  hub.write_current(k, 1);
  hub.set_required(k, 2);
  CHECK(current_events == 1);
  CHECK(required_events == 1);
  hub.unsubscribe(id);
  hub.write_current(k, 5);
  CHECK(current_events == 1);          // no more events after unsubscribe
}

static void test_concurrency() {
  MemoryHub hub;
  const int N = 200;
  for (int i = 0; i < N; ++i) hub.define(TagKey{5, static_cast<uint32_t>(i)}, {});
  std::atomic<bool> go{false};
  auto writer = [&] {
    while (!go) {
    }
    for (int r = 0; r < 500; ++r)
      for (int i = 0; i < N; ++i) hub.write_current(TagKey{5, static_cast<uint32_t>(i)}, r);
  };
  auto reader = [&] {
    while (!go) {
    }
    Sample s;
    for (int r = 0; r < 500; ++r)
      for (int i = 0; i < N; ++i) hub.read(TagKey{5, static_cast<uint32_t>(i)}, s);
  };
  std::vector<std::thread> ts;
  ts.emplace_back(writer);
  ts.emplace_back(writer);
  ts.emplace_back(reader);
  ts.emplace_back(reader);
  go = true;
  for (auto& t : ts) t.join();
  CHECK(hub.size() == N);              // survived concurrent access, no corruption/crash
}

int main() {
  test_tagkey_roundtrip();
  test_define_read_write();
  test_engineering_scaling();
  test_required_setpoints();
  test_module_status();
  test_subscriptions();
  test_concurrency();
  std::printf("osodb: all %d checks passed\n", g_checks);
  return 0;
}
