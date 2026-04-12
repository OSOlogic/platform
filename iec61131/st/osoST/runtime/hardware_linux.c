/*
 * osoLogic — IEC 61131-3 Structured Text Runtime
 * hardware_linux.c — Linux HAL: GPIO (libgpiod), timerfd, Modbus TCP stubs
 *
 * Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
 *               Ibercomp SL, Roig Borrell SL
 *
 * Part of the osoLogic open-source PLC project — osologic.org
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * How to use / Cómo usar:
 *   1. Add trap declarations to your hardware.cfg:
 *      procedure gpio_write(pin:long; value:long) trap #10
 *      function  gpio_read(pin:long) : long trap #11
 *      procedure modbus_write_coil(addr:long; value:long) trap #20
 *
 *   2. Compile with libgpiod:
 *      gcc -O2 osoruntime.c pcodevm.c hardware_linux.c -lgpiod -lm -o osoruntime
 *
 *   3. Define hardware_read_inputs() / hardware_write_outputs() to map
 *      physical I/O to VM global variables before/after each scan cycle.
 *
 * 1. Añadir declaraciones de trap a hardware.cfg
 * 2. Compilar con libgpiod
 * 3. Definir hardware_read_inputs / hardware_write_outputs para mapear
 *    E/S físicas a variables globales de la VM antes/después del ciclo.
 *
 * Build options / Opciones de compilación:
 *   -DUSE_GPIOD   Enable libgpiod GPIO support
 *   -DUSE_MODBUS  Enable Modbus TCP (requires libmodbus)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "pcodevm.h"

#ifdef USE_GPIOD
#  include <gpiod.h>
   static struct gpiod_chip *g_chip = NULL;
#endif

#ifdef USE_MODBUS
#  include <modbus/modbus.h>
   static modbus_t *g_modbus = NULL;
#endif

/* ================================================================
 * HAL initialization / Inicialización del HAL
 * ================================================================ */

/**
 * Initialize Linux hardware resources.
 * Call once before entering the scan loop.
 *
 * Inicializa recursos hardware Linux.
 * Llamar una vez antes de entrar al bucle de scan.
 */
void hardware_linux_init(void) {
#ifdef USE_GPIOD
    g_chip = gpiod_chip_open_by_name("gpiochip0");
    if (!g_chip) { perror("gpiod_chip_open"); exit(1); }
    fprintf(stderr, "[HAL] GPIO chip opened\n");
#endif

#ifdef USE_MODBUS
    g_modbus = modbus_new_tcp("192.168.1.1", 502);
    if (!g_modbus || modbus_connect(g_modbus) < 0) {
        fprintf(stderr, "[HAL] Modbus connect failed\n");
        /* continue — non-fatal for demo */
    } else {
        fprintf(stderr, "[HAL] Modbus TCP connected\n");
    }
#endif
}

/**
 * Release Linux hardware resources.
 * Call once after the scan loop exits.
 *
 * Libera recursos hardware Linux.
 * Llamar una vez tras salir del bucle de scan.
 */
void hardware_linux_deinit(void) {
#ifdef USE_GPIOD
    if (g_chip) { gpiod_chip_close(g_chip); g_chip = NULL; }
#endif
#ifdef USE_MODBUS
    if (g_modbus) { modbus_close(g_modbus); modbus_free(g_modbus); g_modbus = NULL; }
#endif
}

/* ================================================================
 * Scan cycle I/O hooks / Hooks de E/S del ciclo de scan
 *
 * These functions are called by osoruntime.c before and after each
 * VM execution. Customize them to map your physical hardware to the
 * VM global variable layout (offsets defined by the ST compiler).
 *
 * Estas funciones son llamadas por osoruntime.c antes y después de
 * cada ejecución de la VM. Personalízalas para mapear tu hardware
 * físico a las variables globales de la VM.
 * ================================================================ */

/**
 * Read physical inputs into VM global variables.
 * Leer entradas físicas en variables globales de la VM.
 *
 * Example (to be adapted to your hardware.cfg variable layout):
 *   vm->ram[DI_OFFSET] = gpio_read_line(0);   // Digital input 0
 *   vm->ram[AI_OFFSET] = adc_read_channel(1); // Analog input 1
 */
void hardware_read_inputs(VM *vm) {
    /* TODO: map physical inputs to vm->ram global offsets.
     * TODO: mapear entradas físicas a offsets globales en vm->ram. */
    (void)vm;
}

/**
 * Write VM global variables to physical outputs.
 * Escribir variables globales de la VM a salidas físicas.
 *
 * Example:
 *   gpio_write_line(1, vm->ram[DO_OFFSET] & 1); // Digital output 1
 */
void hardware_write_outputs(VM *vm) {
    /* TODO: map vm->ram global offsets to physical outputs.
     * TODO: mapear offsets globales de vm->ram a salidas físicas. */
    (void)vm;
}

/* ================================================================
 * Trap dispatcher / Dispatcher de traps
 * ================================================================ */

/**
 * Linux hardware trap dispatcher.
 * Trap IDs correspond to declarations in hardware.cfg.
 *
 * Dispatcher de traps hardware Linux.
 * Los IDs de trap corresponden a declaraciones en hardware.cfg.
 */
void hardware(VM *vm, uint8_t trap_id) {
    switch (trap_id) {

        /* ── trap #10: procedure gpio_write(pin:long; value:long) ─── */
        case 10: {
            int32_t value = popi(vm, VT_I32);
            int32_t pin   = popi(vm, VT_I32);
#ifdef USE_GPIOD
            if (g_chip) {
                struct gpiod_line *line = gpiod_chip_get_line(g_chip, (unsigned)pin);
                if (line) {
                    gpiod_line_request_output(line, "osoLogic", value ? 1 : 0);
                    gpiod_line_set_value(line, value ? 1 : 0);
                }
            }
#else
            fprintf(stderr, "[HAL] gpio_write(pin=%d, value=%d) [stub]\n", pin, value);
#endif
            return;
        }

        /* ── trap #11: function gpio_read(pin:long) : long ────────── */
        case 11: {
            int32_t pin = popi(vm, VT_I32);
            int32_t val = 0;
#ifdef USE_GPIOD
            if (g_chip) {
                struct gpiod_line *line = gpiod_chip_get_line(g_chip, (unsigned)pin);
                if (line) {
                    gpiod_line_request_input(line, "osoLogic");
                    val = gpiod_line_get_value(line);
                }
            }
#else
            fprintf(stderr, "[HAL] gpio_read(pin=%d) -> 0 [stub]\n", pin);
#endif
            pushi(vm, VT_I32, val);
            return;
        }

        /* ── trap #12: function millis() : long ────────────────────── */
        case 12: {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int32_t ms = (int32_t)((ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL));
            pushi(vm, VT_I32, ms);
            return;
        }

        /* ── trap #20: procedure modbus_write_coil(addr:long; val:long) */
        case 20: {
            int32_t value = popi(vm, VT_I32);
            int32_t addr  = popi(vm, VT_I32);
#ifdef USE_MODBUS
            if (g_modbus) modbus_write_bit(g_modbus, addr, value ? 1 : 0);
#else
            fprintf(stderr,"[HAL] modbus_write_coil(addr=%d,val=%d) [stub]\n",addr,value);
#endif
            return;
        }

        /* ── trap #21: function modbus_read_coil(addr:long) : long ── */
        case 21: {
            int32_t addr = popi(vm, VT_I32);
            int32_t val  = 0;
#ifdef USE_MODBUS
            if (g_modbus) { uint8_t b=0; modbus_read_bits(g_modbus,addr,1,&b); val=b; }
#else
            fprintf(stderr,"[HAL] modbus_read_coil(addr=%d) -> 0 [stub]\n", addr);
#endif
            pushi(vm, VT_I32, val);
            return;
        }

        default:
            fprintf(stderr, "ERROR: unsupported TRAP #%u\n", (unsigned)trap_id);
            exit(1);
    }
}
