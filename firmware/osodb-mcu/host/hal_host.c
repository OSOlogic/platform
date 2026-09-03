/*
 * hal_host — a mock HAL for the desktop simulator and tests. Implements hal.h against
 * in-memory pin state plus a tiny first-order thermal "plant" so the PID visibly
 * converges: the heater PWM heats a mass that cools toward ambient. No hardware needed.
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "hal.h"
#include <string.h>

#define NPINS 64

static bool     g_gpio[NPINS];
static double   g_duty[NPINS];       /* last PWM duty per pin */
static uint32_t g_ms = 0;

/* toy plant state */
static double sim_temp = 20.0;       /* °C */
static double sim_heat_pin_duty = 0; /* duty driving the heater */

/* ADC scale must match the board profile's temp mapping (raw 0..4095 <-> -40..125 °C). */
static uint16_t temp_to_raw(double c)
{
    double raw = (c - (-40.0)) / (125.0 - (-40.0)) * 4095.0;
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (uint16_t)raw;
}

void hal_gpio_config(hal_io_t pin, bool is_output, bool active_low)
{
    (void)pin; (void)is_output; (void)active_low;
}
void hal_pwm_config(hal_io_t pin) { (void)pin; }
void hal_adc_config(hal_io_t channel) { (void)channel; }

bool hal_gpio_read(hal_io_t pin)
{
    return (pin >= 0 && pin < NPINS) ? g_gpio[pin] : false;
}
void hal_gpio_write(hal_io_t pin, bool level)
{
    if (pin >= 0 && pin < NPINS) g_gpio[pin] = level;
}

uint16_t hal_adc_read(hal_io_t channel)
{
    (void)channel;              /* single simulated channel: the plant temperature */
    return temp_to_raw(sim_temp);
}

void hal_pwm_write(hal_io_t pin, double duty_0_1)
{
    if (pin >= 0 && pin < NPINS) g_duty[pin] = duty_0_1;
    sim_heat_pin_duty = duty_0_1;   /* the heater */
}

uint32_t hal_now_ms(void) { return g_ms; }

/* --- simulator-only helpers (not part of hal.h) --- */
void hal_host_set_gpio(hal_io_t pin, bool level)
{
    if (pin >= 0 && pin < NPINS) g_gpio[pin] = level;
}

/* advance the mock clock and the toy thermal plant by dt seconds */
void hal_host_tick(double dt)
{
    const double ambient = 20.0, heat_gain = 60.0, cool = 0.4;
    sim_temp += dt * (heat_gain * sim_heat_pin_duty - cool * (sim_temp - ambient));
    g_ms += (uint32_t)(dt * 1000.0 + 0.5);
}

double hal_host_temp(void) { return sim_temp; }
