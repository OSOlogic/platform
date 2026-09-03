/*
 * osodb-embedded port: Zephyr locking (a global mutex around the tag table).
 * Build the core with -DOSODB_PORT_HEADER=\"osodb_port_zephyr.h\".
 * osodb_mtx is defined + initialised once in main.c.
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#ifndef OSODB_PORT_ZEPHYR_H
#define OSODB_PORT_ZEPHYR_H
#include <zephyr/kernel.h>
extern struct k_mutex osodb_mtx;
#define OSODB_LOCK()   k_mutex_lock(&osodb_mtx, K_FOREVER)
#define OSODB_UNLOCK() k_mutex_unlock(&osodb_mtx)
#endif
