/*
 * hal_zephyr — SCAFFOLD implementation of hal.h on Zephyr.
 *
 * Zephyr addresses GPIO/ADC/PWM through devicetree specs, not flat pin numbers, so the
 * board.yaml pin ids are mapped here to `gpio_dt_spec` / PWM / ADC channels via a small
 * table the integrator fills from the board's devicetree (aliases/overlay). One LED
 * alias is wired as a worked example; complete the rest per board. This is the only
 * file that changes when porting to a new Zephyr board — the control core does not.
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "hal.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* Example: map board.yaml gpio pin 25 -> the board's "led0" alias. Extend this table
 * (and add ADC/PWM equivalents) for every pin your profile uses. */
static const struct gpio_dt_spec led0 =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

static const struct gpio_dt_spec *gpio_lookup(hal_io_t pin)
{
    switch (pin) {
    case 25: return &led0;        /* machine.run in the rp2040_demo profile */
    default: return NULL;         /* TODO: add the rest for your board */
    }
}

void hal_gpio_config(hal_io_t pin, bool is_output, bool active_low)
{
    const struct gpio_dt_spec *g = gpio_lookup(pin);
    if (!g || !device_is_ready(g->port)) {
        return;
    }
    gpio_flags_t f = is_output ? GPIO_OUTPUT_INACTIVE : GPIO_INPUT;
    if (active_low) {
        f |= GPIO_ACTIVE_LOW;
    }
    gpio_pin_configure_dt(g, f);
}

bool hal_gpio_read(hal_io_t pin)
{
    const struct gpio_dt_spec *g = gpio_lookup(pin);
    return (g && device_is_ready(g->port)) ? gpio_pin_get_dt(g) > 0 : false;
}

void hal_gpio_write(hal_io_t pin, bool level)
{
    const struct gpio_dt_spec *g = gpio_lookup(pin);
    if (g && device_is_ready(g->port)) {
        gpio_pin_set_dt(g, level ? 1 : 0);
    }
}

/* TODO: implement against the ADC / PWM drivers for your board (adc_dt_spec, pwm_dt_spec). */
void     hal_pwm_config(hal_io_t pin) { ARG_UNUSED(pin); }
void     hal_adc_config(hal_io_t channel) { ARG_UNUSED(channel); }
uint16_t hal_adc_read(hal_io_t channel) { ARG_UNUSED(channel); return 0; }
void     hal_pwm_write(hal_io_t pin, double duty_0_1) { ARG_UNUSED(pin); ARG_UNUSED(duty_0_1); }

uint32_t hal_now_ms(void) { return k_uptime_get_32(); }
