/*
 * osodb-embedded implementation — see osodb_embedded.h.
 * Pure C99, no heap. Linear table (dozens of tags on an MCU); id lookup by compare.
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "osodb_embedded.h"
#include <string.h>

/*
 * Platform locking. A target supplies OSODB_LOCK()/OSODB_UNLOCK() through a port header
 * (pass -DOSODB_PORT_HEADER=\"osodb_port_zephyr.h\" etc. at build time) — the core file
 * itself never changes, which is what keeps it portable to any target (STM32, RP2040,
 * ESP32, nRF, Rockchip M-cores, …). Default: no-op (single-thread / host tests).
 */
#ifdef OSODB_PORT_HEADER
#include OSODB_PORT_HEADER
#endif
#ifndef OSODB_LOCK
#define OSODB_LOCK()   ((void)0)
#endif
#ifndef OSODB_UNLOCK
#define OSODB_UNLOCK() ((void)0)
#endif

static uint32_t db_bump(osodb_t *db)
{
    return ++db->rev;   /* monotonic; also the per-tag seq stamp */
}

void osodb_init(osodb_t *db)
{
    memset(db, 0, sizeof(*db));
}

int osodb_index(const osodb_t *db, const char *id)
{
    for (uint16_t i = 0; i < db->count; i++) {
        if (strncmp(db->tags[i].id, id, OSODB_ID_LEN) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int osodb_define(osodb_t *db, const char *id, osodb_type_t type,
                 osodb_access_t access, double initial)
{
    if (!id || id[0] == '\0' || strlen(id) >= OSODB_ID_LEN) {
        return -1;
    }
    if (osodb_index(db, id) >= 0) {
        return -1;   /* no duplicates */
    }
    if (db->count >= OSODB_MAX_TAGS) {
        return -1;   /* table full — sized at compile time on purpose */
    }
    OSODB_LOCK();
    int idx = db->count++;
    osodb_tag_t *t = &db->tags[idx];
    memset(t, 0, sizeof(*t));
    strncpy(t->id, id, OSODB_ID_LEN - 1);
    t->type = (uint8_t)type;
    t->access = (uint8_t)access;
    t->value = initial;
    t->quality = 1;
    t->seq = db_bump(db);
    OSODB_UNLOCK();
    return idx;
}

static bool idx_ok(const osodb_t *db, int idx)
{
    return idx >= 0 && idx < (int)db->count;
}

bool osodb_set_value(osodb_t *db, int idx, double v, uint32_t ts_ms)
{
    if (!idx_ok(db, idx)) {
        return false;
    }
    OSODB_LOCK();
    osodb_tag_t *t = &db->tags[idx];
    bool changed = (t->value != v);
    t->value = v;
    t->ts_ms = ts_ms;
    t->quality = 1;
    if (changed) {
        t->seq = db_bump(db);
    }
    OSODB_UNLOCK();
    return changed;
}

bool osodb_set_required(osodb_t *db, int idx, double v, uint32_t ts_ms)
{
    if (!idx_ok(db, idx)) {
        return false;
    }
    OSODB_LOCK();
    osodb_tag_t *t = &db->tags[idx];
    bool changed = (!t->has_required || t->required != v);
    t->required = v;
    t->has_required = 1;
    t->ts_ms = ts_ms;
    if (changed) {
        t->seq = db_bump(db);
    }
    OSODB_UNLOCK();
    return changed;
}

void osodb_clear_required(osodb_t *db, int idx)
{
    if (!idx_ok(db, idx)) {
        return;
    }
    OSODB_LOCK();
    db->tags[idx].has_required = 0;
    OSODB_UNLOCK();
}

double osodb_get_value(const osodb_t *db, int idx)
{
    return idx_ok(db, idx) ? db->tags[idx].value : 0.0;
}

double osodb_get_required(const osodb_t *db, int idx, bool *has_required)
{
    if (!idx_ok(db, idx)) {
        if (has_required) {
            *has_required = false;
        }
        return 0.0;
    }
    if (has_required) {
        *has_required = db->tags[idx].has_required != 0;
    }
    return db->tags[idx].required;
}

uint16_t osodb_count(const osodb_t *db)
{
    return db->count;
}

const osodb_tag_t *osodb_at(const osodb_t *db, int idx)
{
    return idx_ok(db, idx) ? &db->tags[idx] : NULL;
}

int osodb_dirty_since(const osodb_t *db, uint32_t since_rev,
                      int *out_idx, int max_out, uint32_t *new_rev)
{
    int n = 0;
    uint32_t hi = since_rev;
    for (uint16_t i = 0; i < db->count; i++) {
        if (db->tags[i].seq > since_rev) {
            if (n < max_out) {
                out_idx[n++] = (int)i;
            }
            if (db->tags[i].seq > hi) {
                hi = db->tags[i].seq;
            }
        }
    }
    if (new_rev) {
        *new_rev = hi;
    }
    return n;
}
