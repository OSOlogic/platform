/*
 * scan — the hard-real-time control step. Target-agnostic: it drives the generated
 * board_map (read inputs -> control -> write outputs) and federation. A target runs
 * it on a pinned, high-priority thread/timer at a fixed period.
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#ifndef OSODB_SCAN_H
#define OSODB_SCAN_H
#include "osodb_embedded.h"

/* One deterministic control cycle: sensors -> apply set-points -> control -> actuators
 * -> publish changes to the hub. dt in seconds, now_ms a monotonic clock. */
void scan_step(osodb_t *db, double dt, uint32_t now_ms);

#endif /* OSODB_SCAN_H */
