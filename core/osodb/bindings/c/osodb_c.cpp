/* ============================================================
   osodb_c.cpp — implementation of the C binding over the C++ osodb hub.

   Copyright (C) 2026 Roig Borrell S.L. · Ibercomp S.L.
   SPDX-License-Identifier: AGPL-3.0-or-later
   ============================================================ */
#include "osodb_c.h"
#include "../../include/osodb.hpp"

using namespace osodb;

extern "C" int osodb_c_read(const char *tag_id, int64_t *out) {
  if (!tag_id || !out) return 0;
  auto key = TagKey::parse(tag_id);
  if (!key) return 0;
  Sample s;
  if (!MemoryHub::instance().read(*key, s)) return 0;
  *out = static_cast<int64_t>(s.raw);
  return 1;
}

extern "C" int osodb_c_write(const char *tag_id, int64_t raw) {
  if (!tag_id) return 0;
  auto key = TagKey::parse(tag_id);
  if (!key) return 0;
  auto &hub = MemoryHub::instance();
  TagMeta meta;
  if (hub.meta(*key, meta) && meta.access == Access::ReadOnly) {
    return 0;  // ACL: writes to a ReadOnly tag are denied
  }
  hub.write_current(*key, static_cast<uint64_t>(raw));
  return 1;
}

extern "C" int osodb_c_take_required(const char *tag_id, int64_t *out) {
  if (!tag_id || !out) return 0;
  auto key = TagKey::parse(tag_id);
  if (!key) return 0;
  uint64_t v = 0;
  if (!MemoryHub::instance().take_required(*key, v)) return 0;
  *out = static_cast<int64_t>(v);
  return 1;
}

extern "C" void osodb_c_define(const char *tag_id, int access) {
  if (!tag_id) return;
  auto key = TagKey::parse(tag_id);
  if (!key) return;
  TagMeta meta;
  meta.access = access ? Access::ReadWrite : Access::ReadOnly;
  MemoryHub::instance().define(*key, meta);
}
