/*
 * hal — the tiny hardware-abstraction interface the generated board_map uses.
 *
 * A board profile (board.yaml) is turned by boardgen into board_map.c, which calls
 * only these functions. Each target implements them once: hal_zephyr.c (GPIO/PWM/ADC
 * via Zephyr device APIs), hal_freertos.c (vendor HAL), hal_host.c (mock, for the
 * host simulator and tests). Porting to any new MCU/SoC = implement hal.h + write a
 * board.yaml. The control core never changes.
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#ifndef OSODB_HAL_H
#define OSODB_HAL_H
#include <stdint.h>
#include <stdbool.h>

/* Opaque pin/channel identifier — the generated map passes the profile's numeric id. */
typedef int hal_io_t;

/* Called once at boot, before any read/write (the generated board_io_init drives it). */
void hal_gpio_config(hal_io_t pin, bool is_output, bool active_low);
void hal_pwm_config(hal_io_t pin);
void hal_adc_config(hal_io_t channel);

/* Digital */
bool hal_gpio_read(hal_io_t pin);
void hal_gpio_write(hal_io_t pin, bool level);

/* Analog / PWM */
uint16_t hal_adc_read(hal_io_t channel);      /* raw counts */
void     hal_pwm_write(hal_io_t pin, double duty_0_1);

/* Monotonic clock in ms since boot (for tag timestamps and scan dt). */
uint32_t hal_now_ms(void);

#endif /* OSODB_HAL_H */
