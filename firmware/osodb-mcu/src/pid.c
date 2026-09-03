/*
 * pid implementation — see pid.h. Portable C99, no allocation.
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "pid.h"

void pid_init(pid_t *p, double kp, double ki, double kd, double out_min, double out_max)
{
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
    p->out_min = out_min;
    p->out_max = out_max;
    pid_reset(p);
}

void pid_reset(pid_t *p)
{
    p->integ = 0.0;
    p->prev_err = 0.0;
    p->primed = 0;
}

double pid_step(pid_t *p, double setpoint, double measured, double dt)
{
    double err = setpoint - measured;
    if (!p->primed) {
        p->prev_err = err;   /* avoid a derivative spike on the first step */
        p->primed = 1;
    }
    double deriv = (dt > 0.0) ? (err - p->prev_err) / dt : 0.0;
    double integ = p->integ + err * dt;

    double out = p->kp * err + p->ki * integ + p->kd * deriv;

    /* clamp + conditional integration (anti-windup): only keep the integral
     * term if the output is not saturated, so it can't wind up past the limits. */
    if (out > p->out_max) {
        out = p->out_max;
    } else if (out < p->out_min) {
        out = p->out_min;
    } else {
        p->integ = integ;
    }
    p->prev_err = err;
    return out;
}
