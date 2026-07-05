/* ============================================================
   osodb_c.h — C binding for the osodb hub (core/osodb).

   A thin, dependency-free C API over the C++ MemoryHub so C code — notably
   the IEC 61131-3 runtime HAL (osoST) — can read/write osodb tags natively
   (nanosecond, in-process) instead of going over REST. osodb owns the
   write-through/write-back sync to MariaDB via its adapters, so callers of
   this API never touch the database directly.

   Tag ids use osodb's canonical form "<module_id>.<io_definition_id>"
   (e.g. "1.5"), matching TagKey::to_string(). ACL is osodb's own per-tag
   Access (ReadOnly / ReadWrite): a write to a ReadOnly tag is rejected here.

   Copyright (C) 2026 Roig Borrell S.L. · Ibercomp S.L.
   SPDX-License-Identifier: AGPL-3.0-or-later
   ============================================================ */
#ifndef OSODB_C_H
#define OSODB_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read a tag's current value (scan input image).
   Returns 1 on success, 0 if the id is malformed or the tag has no reading. */
int osodb_c_read(const char *tag_id, int64_t *out);

/* Write a tag's current value (scan output image). ACL-enforced: a ReadOnly
   tag is rejected. Returns 1 if written, 0 if denied or the id is malformed. */
int osodb_c_write(const char *tag_id, int64_t raw);

/* Take one pending required set-point (external control, e.g. SQL UPDATE
   required_value). Returns 1 and sets *out if one was pending, else 0. */
int osodb_c_take_required(const char *tag_id, int64_t *out);

/* Define/declare a tag with an access level (0 = ReadOnly, 1 = ReadWrite).
   Normally the module/gateways define tags from the DB model; this is a
   convenience for standalone runtimes and tests. */
void osodb_c_define(const char *tag_id, int access);

#ifdef __cplusplus
}
#endif

#endif /* OSODB_C_H */
