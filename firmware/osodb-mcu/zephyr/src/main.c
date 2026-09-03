/*
 * Zephyr entry point for the OSOlogic MCU tier.
 * A pinned, cooperative scan thread runs the control loop at a fixed period; a second
 * thread reads federation lines from the console UART ("SET <id> <value>"). The tag
 * table is protected by osodb_mtx (see osodb_port_zephyr.h).
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include "osodb_embedded.h"
#include "board_map.h"
#include "scan.h"
#include "federation.h"

#define SCAN_MS 100

struct k_mutex osodb_mtx;      /* referenced by osodb_port_zephyr.h */
static osodb_t db;

/* federation output over the console UART */
void federation_emit(const char *line)
{
    printk("%s\n", line);
}

static void federation_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    console_getline_init();
    for (;;) {
        char *line = console_getline();          /* blocks on the console UART */
        if (line) {
            federation_handle_line(&db, line, k_uptime_get_32());
        }
    }
}
K_THREAD_DEFINE(fed_tid, 1024, federation_thread, NULL, NULL, NULL, 7, 0, 0);

int main(void)
{
    k_mutex_init(&osodb_mtx);
    osodb_init(&db);
    board_io_define(&db);
    board_io_init();

    uint32_t last = k_uptime_get_32();
    for (;;) {
        uint32_t now = k_uptime_get_32();
        double dt = (now - last) / 1000.0;
        last = now;
        scan_step(&db, dt, now);
        k_msleep(SCAN_MS);
    }
    return 0;
}
