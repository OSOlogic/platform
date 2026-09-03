/*
 * scan implementation — see scan.h.
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "scan.h"
#include "board_map.h"
#include "federation.h"

void scan_step(osodb_t *db, double dt, uint32_t now_ms)
{
    /* 1. sensors -> osodb */
    board_io_read_inputs(db, now_ms);

    /* 2. apply hub set-points: any RW tag with a pending set-point follows it
     *    (control loops below then overwrite the CVs they own). */
    int count = osodb_count(db);
    for (int i = 0; i < count; i++) {
        const osodb_tag_t *t = osodb_at(db, i);
        if (t && t->access == OSODB_RW && t->has_required) {
            osodb_set_value(db, i, t->required, now_ms);
        }
    }

    /* 3. hard-real-time control loops (PID) overwrite their control variables */
    board_control_step(db, dt, now_ms);

    /* 4. osodb -> actuators */
    board_io_write_outputs(db);

    /* 5. push whatever changed to the network hub */
    federation_publish_dirty(db);
}
