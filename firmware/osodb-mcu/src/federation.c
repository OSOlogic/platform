/*
 * Federation implementation — see federation.h.
 * Portable C99. Output goes through federation_emit(); the default prints to stdout
 * (host / native_sim / any console), and a target can override it (weak symbol) to
 * push over UART, MQTT, CAN, etc. Inbound lines are fed by the target's link reader.
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "federation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Override on a target to send over its real link (UART/MQTT/CAN). */
#if defined(__GNUC__)
__attribute__((weak))
#endif
void federation_emit(const char *line)
{
    fputs(line, stdout);
    fputc('\n', stdout);
}

int federation_publish_dirty(osodb_t *db)
{
    static uint32_t last_rev = 0;   /* the gateway's high-water mark */
    int idx[OSODB_MAX_TAGS];
    uint32_t new_rev = last_rev;
    int n = osodb_dirty_since(db, last_rev, idx, OSODB_MAX_TAGS, &new_rev);
    char line[80];
    for (int i = 0; i < n; i++) {
        const osodb_tag_t *t = osodb_at(db, idx[i]);
        if (!t) {
            continue;
        }
        snprintf(line, sizeof(line), "PUB %s %.6g %u", t->id, t->value, (unsigned)t->seq);
        federation_emit(line);
    }
    last_rev = new_rev;
    return n;
}

int federation_handle_line(osodb_t *db, const char *line, uint32_t now_ms)
{
    if (!line) {
        return -1;
    }
    char id[OSODB_ID_LEN];
    double val;
    /* "SET <id> <value>" */
    if (sscanf(line, "SET %23s %lf", id, &val) == 2) {
        int idx = osodb_index(db, id);
        if (idx < 0) {
            return -2;                       /* unknown tag */
        }
        const osodb_tag_t *t = osodb_at(db, idx);
        if (!t || t->access != OSODB_RW) {
            return -3;                       /* read-only: reject set-point */
        }
        osodb_set_required(db, idx, val, now_ms);
        return 0;
    }
    return -1;                               /* not a command we handle */
}
