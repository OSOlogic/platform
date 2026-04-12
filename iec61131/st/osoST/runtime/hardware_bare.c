/*
 * osoLogic — IEC 61131-3 Structured Text Runtime
 * hardware_bare.c — Bare-metal MCU HAL template
 *
 * Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
 *               Ibercomp SL, Roig Borrell SL
 *
 * Part of the osoLogic open-source PLC project — osologic.org
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Supported targets / Targets soportados:
 *   STM32  (ARM Cortex-M, HAL via STM32CubeMX)
 *   RP2040 (Raspberry Pi Pico SDK)
 *   ESP32  (ESP-IDF)
 *   Generic bare-metal with custom BSP
 *
 * How to use / Cómo usar:
 *   1. Copy this file to your MCU project.
 *      Copiar este fichero a tu proyecto MCU.
 *
 *   2. Enable the correct #ifdef block below for your platform.
 *      Activar el bloque #ifdef correcto para tu plataforma.
 *
 *   3. Implement hardware_read_inputs() and hardware_write_outputs()
 *      to map physical I/O to VM global variable offsets.
 *      Implementar hardware_read_inputs() y hardware_write_outputs()
 *      para mapear E/S físicas a offsets de variables globales de la VM.
 *
 *   4. Implement the scan cycle in your main():
 *
 *      // Timer ISR or RTOS task
 *      void scan_cycle_cb(void) {
 *          hardware_read_inputs(&vm);
 *          vm_reset(&vm);
 *          vm_start(&vm);
 *          run_vm(&vm);
 *          hardware_write_outputs(&vm);
 *      }
 *
 * Build / Compilar:
 *   # STM32 example with arm-none-eabi-gcc:
 *   arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O2 -DTARGET_STM32 \
 *       pcodevm.c hardware_bare.c main.c -lm -o firmware.elf
 *
 *   # RP2040 (add to CMakeLists.txt):
 *   target_sources(my_target PRIVATE pcodevm.c hardware_bare.c)
 *   target_compile_definitions(my_target PRIVATE TARGET_RP2040)
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pcodevm.h"

/* ================================================================
 * Platform includes / Includes de plataforma
 * ================================================================ */

#if defined(TARGET_STM32)
#  include "stm32f4xx_hal.h"   /* adjust for your STM32 family */
   /* extern TIM_HandleTypeDef htim6; */

#elif defined(TARGET_RP2040)
#  include "pico/stdlib.h"
#  include "hardware/gpio.h"
#  include "hardware/adc.h"
#  include "hardware/timer.h"

#elif defined(TARGET_ESP32)
#  include "driver/gpio.h"
#  include "driver/adc.h"
#  include "esp_timer.h"

#else
#  warning "No target defined. Using stub implementations."
#  define TARGET_STUB
#endif

/* ================================================================
 * Scan cycle timer (bare-metal ISR / RTOS task)
 * Temporizador de ciclo de scan (ISR bare-metal / tarea RTOS)
 * ================================================================ */

/*
 * Typical setup for a 10 ms scan cycle on STM32 using TIM6 (basic timer):
 *
 *   // In main() after HAL_Init():
 *   HAL_TIM_Base_Start_IT(&htim6);   // Configure TIM6 for 10 ms period
 *
 *   // Timer ISR (in stm32f4xx_it.c or similar):
 *   void TIM6_DAC_IRQHandler(void) {
 *       HAL_TIM_IRQHandler(&htim6);
 *   }
 *   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
 *       if (htim->Instance == TIM6) scan_trigger = 1;
 *   }
 *
 *   // Main loop:
 *   while (1) {
 *       if (scan_trigger) {
 *           scan_trigger = 0;
 *           hardware_read_inputs(&vm);
 *           vm_reset(&vm); vm_start(&vm); run_vm(&vm);
 *           hardware_write_outputs(&vm);
 *       }
 *       __WFI();  // sleep until next IRQ
 *   }
 */

/* ================================================================
 * Scan cycle I/O hooks / Hooks de E/S del ciclo de scan
 * ================================================================ */

/**
 * Read physical inputs into VM global variables at the start of each scan.
 * Leer entradas físicas en variables globales de la VM al inicio del scan.
 *
 * The offsets (e.g. OFFSET_DI0) are determined by the ST compiler
 * based on your VAR_GLOBAL declarations. Check the .asm or .map output.
 *
 * Los offsets los determina el compilador ST según tus declaraciones
 * VAR_GLOBAL. Consulta la salida .asm o .map.
 */
void hardware_read_inputs(VM *vm) {
#if defined(TARGET_STM32)
    /* Example: map GPIOB pins 0-7 to global BOOL variables */
    /* Ejemplo: mapear pines GPIOB 0-7 a variables globales BOOL */
    /* uint16_t pins = GPIOB->IDR;
    for (int i = 0; i < 8; i++) {
        vm->ram[OFFSET_DI0 + i] = (pins >> i) & 1;
    } */

#elif defined(TARGET_RP2040)
    /* Example: read GPIO 0-7 */
    /* uint32_t pins = gpio_get_all();
    for (int i = 0; i < 8; i++) {
        vm->ram[OFFSET_DI0 + i] = (pins >> i) & 1;
    } */

#elif defined(TARGET_ESP32)
    /* Example: read GPIO levels */
    /* vm->ram[OFFSET_DI0] = gpio_get_level(GPIO_NUM_2); */

#else
    /* Stub */
    (void)vm;
#endif
}

/**
 * Write VM global variables to physical outputs at the end of each scan.
 * Escribir variables globales de la VM a salidas físicas al fin del scan.
 */
void hardware_write_outputs(VM *vm) {
#if defined(TARGET_STM32)
    /* Example: write 8 digital outputs to GPIOC */
    /* uint16_t mask = 0;
    for (int i = 0; i < 8; i++) {
        if (vm->ram[OFFSET_DO0 + i]) mask |= (1u << i);
    }
    GPIOC->ODR = mask; */

#elif defined(TARGET_RP2040)
    /* Example: set GPIO 8-15 from DO variables */
    /* for (int i = 0; i < 8; i++) {
        gpio_put(8 + i, vm->ram[OFFSET_DO0 + i] ? 1 : 0);
    } */

#elif defined(TARGET_ESP32)
    /* vm->ram[OFFSET_DO0] ? gpio_set_level(GPIO_NUM_4, 1) : gpio_set_level(GPIO_NUM_4, 0); */

#else
    (void)vm;
#endif
}

/* ================================================================
 * Trap dispatcher / Dispatcher de traps
 * ================================================================ */

/**
 * Bare-metal hardware trap dispatcher.
 * Dispatcher de traps hardware bare-metal.
 */
void hardware(VM *vm, uint8_t trap_id) {
    switch (trap_id) {

        /* ── trap #10: procedure gpio_write(pin:long; value:long) ─── */
        case 10: {
            int32_t value = popi(vm, VT_I32);
            int32_t pin   = popi(vm, VT_I32);
#if defined(TARGET_RP2040)
            gpio_put((uint32_t)pin, value ? 1 : 0);
#elif defined(TARGET_ESP32)
            gpio_set_level((gpio_num_t)pin, value ? 1 : 0);
#elif defined(TARGET_STM32)
            HAL_GPIO_WritePin(GPIOB, (uint16_t)(1u<<pin), value ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
            (void)pin; (void)value;
#endif
            return;
        }

        /* ── trap #11: function gpio_read(pin:long) : long ────────── */
        case 11: {
            int32_t pin = popi(vm, VT_I32);
            int32_t val = 0;
#if defined(TARGET_RP2040)
            val = gpio_get((uint32_t)pin) ? 1 : 0;
#elif defined(TARGET_ESP32)
            val = gpio_get_level((gpio_num_t)pin);
#elif defined(TARGET_STM32)
            val = HAL_GPIO_ReadPin(GPIOB, (uint16_t)(1u<<pin)) == GPIO_PIN_SET ? 1 : 0;
#else
            (void)pin;
#endif
            pushi(vm, VT_I32, val);
            return;
        }

        /* ── trap #12: function millis() : long ────────────────────── */
        case 12: {
            int32_t ms = 0;
#if defined(TARGET_STM32)
            ms = (int32_t)HAL_GetTick();
#elif defined(TARGET_RP2040)
            ms = (int32_t)(time_us_64() / 1000ULL);
#elif defined(TARGET_ESP32)
            ms = (int32_t)(esp_timer_get_time() / 1000LL);
#endif
            pushi(vm, VT_I32, ms);
            return;
        }

        default:
            /* On bare-metal: log and halt rather than exit() */
            /* En bare-metal: registrar y detener en lugar de exit() */
#if defined(TARGET_STM32)
            while (1) { HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5); HAL_Delay(100); }
#elif defined(TARGET_RP2040)
            while (1) { gpio_put(25, 1); sleep_ms(100); gpio_put(25, 0); sleep_ms(100); }
#else
            while (1) {}  /* spin — watchdog should reset */
#endif
    }
}
