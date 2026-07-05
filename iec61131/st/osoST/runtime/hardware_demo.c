/*
 * osoLogic — IEC 61131-3 Structured Text Runtime
 * hardware_demo.c — Demo HAL: BACnet attribute stubs, cpu() introspection,
 *                   Mandelbrot native acceleration
 *
 * Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
 *               Ibercomp SL, Roig Borrell SL
 *
 * Part of the osoLogic open-source PLC project — osologic.org
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file implements the hardware() trap dispatcher for demo/development.
 * For production, replace with hardware_linux.c or hardware_bare.c.
 * / Este fichero implementa el dispatcher de traps para demo/desarrollo.
 * Para producción, sustituir por hardware_linux.c o hardware_bare.c.
 *
 * Current traps / Traps actuales:
 *   #1  setBACnetAttribute(name:STRING, value:LONG) — prints to stdout
 *   #17 mandelbrot_iters(cx:FLOAT, cy:FLOAT, max_iter:LONG):LONG — native accel
 *   #18 cpu() — prints VM internal state for diagnostics
 */



#include <stdio.h>
#include <stdlib.h>
#include "pcodevm.h"
#include "osodb_tags.h"   /* osodb tag I/O bridge (trap #30/#31, ACL-enforced) */

/**
 * @file hardware.c
 * @brief Implementación nativa de traps de hardware para la VM.
 *
 * Este módulo concentra las primitivas de alto nivel que no forman parte
 * del set de opcodes base. El compilador genera `TRAP #n` y aquí se resuelve
 * cada ID en código C nativo.
 */

/**
 * @brief Núcleo nativo de iteraciones Mandelbrot.
 * @param cx Coordenada real del punto.
 * @param cy Coordenada imaginaria del punto.
 * @param max_iter Límite máximo de iteraciones.
 * @return Iteraciones realizadas hasta divergir o agotar límite.
 */
static int32_t mandelbrot_iters_native(float cx, float cy, int32_t max_iter) {
    int32_t lim = (max_iter < 0) ? 0 : max_iter;
    float x = 0.0f, y = 0.0f;
    int32_t it = 0;
    while (it < lim) {
        float x2 = x * x;
        float y2 = y * y;
        if (x2 + y2 > 4.0f) break;
        float xy = x * y;
        x = x2 - y2 + cx;
        y = 2.0f * xy + cy;
        it++;
    }
    return it;
}

/**
 * @brief Despachador de traps de hardware.
 *
 * Contrato:
 * - Lee parámetros desde la pila VM (último parámetro en tope).
 * - Ejecuta operación nativa asociada a `trap_id`.
 * - Si aplica, deja valor de retorno en la pila VM.
 *
 * Traps actuales:
 * - `#1`  : `setBACnetAttribute(name: STRING, value: LONG)` (demo I/O).
 * - `#17` : `mandelbrot_iters(cx: FLOAT, cy: FLOAT, max_iter: LONG): LONG`.
 */
void hardware(VM *vm, uint8_t trap_id) {
    switch (trap_id) {
        case 1: { // setBACnetAttribute(name:STRING, value:LONG)
            int32_t value = popi(vm, VT_I32);
            int32_t name_ptr = popi(vm, VT_STR) & 0xFFFF;
            char name[256];
            read_cstr(vm, name_ptr, name, sizeof(name));
            vm_release_temp_string(vm, name_ptr);
            printf("SetBACnetAtribute:%s=%d\n", name, value);
            return;
        }
        case 17: { // function mandelbrot_iters(cx:FLOAT, cy:FLOAT, max_iter:LONG):LONG
            int32_t max_iter = popi(vm, VT_I32);
            float cy = popf(vm);
            float cx = popf(vm);
            pushi(vm, VT_I32, mandelbrot_iters_native(cx, cy, max_iter));
            return;
        }
        case 18: { // procedure cpu()
            int32_t stack_cap = vm->stack_limit - vm->global_bytes;
            if (stack_cap < 0) stack_cap = 0;
            int32_t stack_used = vm->sp - vm->global_bytes;
            if (stack_used < 0) stack_used = 0;
            int32_t temp_used = vm->str_temp_limit - vm->str_temp_top;
            if (temp_used < 0) temp_used = 0;
            int32_t free_gap = vm->str_temp_top - vm->sp;
            if (free_gap < 0) free_gap = 0;
            printf("[CPU]\n");
            printf("pc=%d sp=%d fp=%d\n", vm->pc, vm->sp, vm->fp);
            printf("globals=0..%d bytes=%d\n", vm->global_bytes > 0 ? vm->global_bytes - 1 : -1, vm->global_bytes);
            printf("work[%d..%d) stack_used=%d temp_used=%d free=%d total=%d\n",
                   vm->global_bytes, vm->stack_limit, stack_used, temp_used, free_gap, stack_cap);
            printf("stack_free_now=%d temp_free_now=%d (zona libre compartida)\n", free_gap, free_gap);
            printf("math_error=%u rng_state=%d running=%u\n",
                   (unsigned) vm->math_error, vm->rng_state, (unsigned) vm->running);
            return;
        }
        /* trap #30: function tag_read(id:long) : long — osodb (ACL) */
        case 30: {
            int32_t id = popi(vm, VT_I32);
            pushi(vm, VT_I32, osodb_tag_read(id));
            return;
        }
        /* trap #31: procedure tag_write(id:long; value:long) — osodb (ACL) */
        case 31: {
            int32_t value = popi(vm, VT_I32);
            int32_t id    = popi(vm, VT_I32);
            osodb_tag_write(id, value);
            return;
        }
        default:
            fprintf(stderr, "ERROR: TRAP no soportado #%u\n", (unsigned) trap_id);
            exit(1);
    }
}

