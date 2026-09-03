/*
 * main_host — desktop simulator for the OSOlogic MCU tier. Runs the SAME code that
 * ships to the MCU (osodb-embedded + generated board_map + scan + PID + federation)
 * against the mock HAL, so the whole chain is exercised with no hardware.
 *
 *   make host && ./build/osodb-mcu-sim
 *
 * It seeds the board's tags, takes a set-point from the "hub" (a federation SET line),
 * runs the control loop, and shows the PID driving the plant to set-point plus the
 * PUB traffic the node would send upstream.
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "osodb_embedded.h"
#include "board_map.h"
#include "federation.h"
#include "scan.h"
#include "hal.h"
#include <stdio.h>

extern void   hal_host_tick(double dt);
extern double hal_host_temp(void);

/* Count federation traffic instead of flooding the console (override the weak default). */
static long g_pub = 0;
static int  g_verbose = 0;
void federation_emit(const char *line)
{
    g_pub++;
    if (g_verbose) printf("    -> %s\n", line);
}

int main(void)
{
    osodb_t db;
    osodb_init(&db);
    board_io_define(&db);
    board_io_init();

    printf("OSOlogic MCU-tier simulator — board '%s', %u tags\n\n", BOARD_NAME, osodb_count(&db));

    /* the hub commands the node over the federation link */
    printf("hub -> node:\n");
    printf("    SET machine.run 1  => %d\n", federation_handle_line(&db, "SET machine.run 1", hal_now_ms()));
    printf("    SET temp.sp 75     => %d\n", federation_handle_line(&db, "SET temp.sp 75", hal_now_ms()));
    printf("    SET temp 999 (RO)  => %d  (rejected, read-only)\n\n",
           federation_handle_line(&db, "SET temp 999", hal_now_ms()));

    const double dt = 0.1;   /* 100 ms scan */
    printf("closed loop (PID temp -> heater), scan %.0f ms:\n", dt * 1000);
    for (int step = 0; step <= 400; step++) {
        hal_host_tick(dt);
        scan_step(&db, dt, hal_now_ms());
        if (step % 50 == 0) {
            bool has;
            double sp = osodb_get_required(&db, osodb_index(&db, "temp.sp"), &has);
            printf("    t=%5.1fs  temp=%6.2f  sp=%.0f  heater=%6.2f%%  run=%.0f\n",
                   hal_now_ms() / 1000.0,
                   osodb_get_value(&db, osodb_index(&db, "temp")), sp,
                   osodb_get_value(&db, osodb_index(&db, "heater")),
                   osodb_get_value(&db, osodb_index(&db, "machine.run")));
        }
    }

    double final_temp = hal_host_temp();
    printf("\nfederation: %ld PUB messages emitted upstream.\n", g_pub);
    printf("result: temp settled at %.2f °C (target 75) — %s\n",
           final_temp, (final_temp > 73.0 && final_temp < 77.0) ? "CONVERGED" : "OUT OF BAND");
    return (final_temp > 73.0 && final_temp < 77.0) ? 0 : 1;
}
