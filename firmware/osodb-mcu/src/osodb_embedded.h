/*
 * osodb-embedded — portable, allocation-free tag store for MCU-class targets (Tier-0).
 *
 * The hard-real-time tier of OSOlogic: a fixed-size tag table sized at compile time,
 * no heap, no stdlib allocation, no RTOS headers. The SAME file compiles under Zephyr,
 * FreeRTOS and a host unit test. It is the on-MCU face of osodb; it federates its tags
 * with the network hub (Linux/x86 "big siblings") over a gateway (MQTT / Modbus / line).
 *
 * Thread-safety: define OSODB_LOCK()/OSODB_UNLOCK() for your platform (Zephyr k_mutex,
 * FreeRTOS mutex/critical section, or no-op for single-thread / host tests) before
 * including this in the .c, or leave them empty.
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#ifndef OSODB_EMBEDDED_H
#define OSODB_EMBEDDED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef OSODB_MAX_TAGS
#define OSODB_MAX_TAGS 64          /* compile-time table size; no allocation ever */
#endif
#ifndef OSODB_ID_LEN
#define OSODB_ID_LEN 24            /* max tag id length, NUL included */
#endif

typedef enum { OSODB_BOOL = 0, OSODB_INT = 1, OSODB_REAL = 2 } osodb_type_t;
typedef enum { OSODB_RO = 0, OSODB_RW = 1 } osodb_access_t;

typedef struct {
    char     id[OSODB_ID_LEN];  /* osodb key, e.g. "2.5" or "motor.speed" */
    double   value;             /* current value */
    double   required;          /* set-point (write target; applied by the scan) */
    uint8_t  type;              /* osodb_type_t */
    uint8_t  access;            /* osodb_access_t */
    uint8_t  has_required;      /* 1 if a set-point is pending/active */
    uint8_t  quality;           /* 0 = bad, 1 = good */
    uint32_t seq;               /* per-tag change counter (federation dirty tracking) */
    uint32_t ts_ms;             /* last update, ms since boot (caller supplies the clock) */
} osodb_tag_t;

typedef struct {
    osodb_tag_t tags[OSODB_MAX_TAGS];
    uint16_t    count;
    uint32_t    rev;            /* global revision: max seq handed out so far */
} osodb_t;

/* lifecycle */
void osodb_init(osodb_t *db);

/* define a tag; returns its index, or -1 if the table is full / id too long / dup */
int  osodb_define(osodb_t *db, const char *id, osodb_type_t type,
                  osodb_access_t access, double initial);

/* lookup by id; returns index or -1 */
int  osodb_index(const osodb_t *db, const char *id);

/* writes; return true if the stored value actually changed */
bool osodb_set_value(osodb_t *db, int idx, double v, uint32_t ts_ms);
bool osodb_set_required(osodb_t *db, int idx, double v, uint32_t ts_ms);
void osodb_clear_required(osodb_t *db, int idx);

/* reads */
double   osodb_get_value(const osodb_t *db, int idx);
double   osodb_get_required(const osodb_t *db, int idx, bool *has_required);
uint16_t osodb_count(const osodb_t *db);
const osodb_tag_t *osodb_at(const osodb_t *db, int idx);

/*
 * Federation: fill out_idx[] with the indices of tags whose seq is newer than
 * since_rev (i.e. changed since the caller last synced). Returns the number written
 * (capped at max_out) and updates *new_rev to the highest rev seen — pass that back
 * as since_rev next time. This is the whole dirty-tracking a gateway needs.
 */
int osodb_dirty_since(const osodb_t *db, uint32_t since_rev,
                      int *out_idx, int max_out, uint32_t *new_rev);

#endif /* OSODB_EMBEDDED_H */
