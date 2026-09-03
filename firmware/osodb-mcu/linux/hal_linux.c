/*
 * hal_linux — SCAFFOLD implementation of hal.h for Linux SBCs (Raspberry Pi, Orange Pi,
 * any Rockchip/AllWinner board) using libgpiod. Proves the board-profile mechanism is not
 * MCU-only: the SAME board_map runs on a Linux node, the OSOlogic tier-1 target.
 *
 * Digital I/O is implemented via libgpiod (v2). Analog (ADC) and PWM on these boards is
 * board-specific — wire an external I2C ADC (e.g. ADS1115) or sysfs/pwmchip here.
 *
 *   cc ... firmware/osodb-mcu/linux/hal_linux.c ... -lgpiod
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "hal.h"
#include <gpiod.h>          /* libgpiod v2 */
#include <time.h>
#include <stdlib.h>

#ifndef OSODB_GPIOCHIP
#define OSODB_GPIOCHIP "/dev/gpiochip0"   /* Orange Pi models may use gpiochip1/…; override */
#endif
#define MAX_LINES 64

static struct gpiod_chip *chip;
static struct gpiod_line_request *req[MAX_LINES];   /* per-line request, indexed by pin */

/* NOTE scaffold: one request per line keeps the mapping trivial; for many lines, batch
 * them into a single line_request for efficiency. */
void hal_gpio_config(hal_io_t pin, bool is_output, bool active_low)
{
    if (!chip) {
        chip = gpiod_chip_open(OSODB_GPIOCHIP);
        if (!chip) return;
    }
    if (pin < 0 || pin >= MAX_LINES) return;

    struct gpiod_line_settings *s = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(s,
        is_output ? GPIOD_LINE_DIRECTION_OUTPUT : GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_active_low(s, active_low);

    struct gpiod_line_config *lc = gpiod_line_config_new();
    unsigned int off = (unsigned int)pin;
    gpiod_line_config_add_line_settings(lc, &off, 1, s);

    struct gpiod_request_config *rc = gpiod_request_config_new();
    gpiod_request_config_set_consumer(rc, "osologic");

    req[pin] = gpiod_chip_request_lines(chip, rc, lc);

    gpiod_request_config_free(rc);
    gpiod_line_config_free(lc);
    gpiod_line_settings_free(s);
}

bool hal_gpio_read(hal_io_t pin)
{
    if (pin < 0 || pin >= MAX_LINES || !req[pin]) return false;
    return gpiod_line_request_get_value(req[pin], (unsigned int)pin) == GPIOD_LINE_VALUE_ACTIVE;
}

void hal_gpio_write(hal_io_t pin, bool level)
{
    if (pin < 0 || pin >= MAX_LINES || !req[pin]) return;
    gpiod_line_request_set_value(req[pin], (unsigned int)pin,
        level ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

/* TODO: analog via an external I2C ADC; PWM via sysfs pwmchip. */
void     hal_pwm_config(hal_io_t pin) { (void)pin; }
void     hal_adc_config(hal_io_t channel) { (void)channel; }
uint16_t hal_adc_read(hal_io_t channel) { (void)channel; return 0; }
void     hal_pwm_write(hal_io_t pin, double duty) { (void)pin; (void)duty; }

uint32_t hal_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
