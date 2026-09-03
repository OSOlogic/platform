/*
 * osodb-embedded port: FreeRTOS locking (a recursive mutex around the tag table).
 * Build the core with -DOSODB_PORT_HEADER=\"osodb_port_freertos.h\".
 * osodb_mtx is created in the FreeRTOS entry point (see freertos/).
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#ifndef OSODB_PORT_FREERTOS_H
#define OSODB_PORT_FREERTOS_H
#include "FreeRTOS.h"
#include "semphr.h"
extern SemaphoreHandle_t osodb_mtx;
#define OSODB_LOCK()   xSemaphoreTakeRecursive(osodb_mtx, portMAX_DELAY)
#define OSODB_UNLOCK() xSemaphoreGiveRecursive(osodb_mtx)
#endif
