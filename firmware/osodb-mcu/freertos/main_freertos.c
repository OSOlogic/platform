/*
 * FreeRTOS port (SCAFFOLD) for the OSOlogic MCU tier — for the smallest targets where
 * Zephyr is too heavy. It reuses the SAME portable core (osodb_embedded / pid / scan /
 * federation); only the RTOS glue and the vendor HAL differ.
 *
 * Build the core with -DOSODB_PORT_HEADER=\"osodb_port_freertos.h\". Provide a hal_*.c
 * for your MCU (STM32 HAL, pico-sdk, ESP-IDF, …) implementing hal.h, and the generated
 * board_map.c from your board.yaml.
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "osodb_embedded.h"
#include "board_map.h"
#include "scan.h"
#include "hal.h"

#define SCAN_MS 100

SemaphoreHandle_t osodb_mtx;    /* referenced by osodb_port_freertos.h */
static osodb_t db;

/* TODO: override federation_emit() to push over your link (UART/CAN/MQTT), and feed
 * inbound "SET <id> <value>" lines to federation_handle_line() from your link RX. */

static void scan_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        scan_step(&db, SCAN_MS / 1000.0, hal_now_ms());
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SCAN_MS));   /* fixed-period, deterministic */
    }
}

int main(void)
{
    osodb_mtx = xSemaphoreCreateRecursiveMutex();
    osodb_init(&db);
    board_io_define(&db);
    board_io_init();

    xTaskCreate(scan_task, "scan", 1024, NULL, configMAX_PRIORITIES - 1, NULL);
    vTaskStartScheduler();
    for (;;) { }        /* never reached */
    return 0;
}
