/*
 * pid — a small, portable PID controller with anti-windup and output clamping.
 * The hard-real-time control primitive of the MCU tier. Gains are plain fields, so
 * an online tuner (or the "AI-improved PID" Enterprise module) can rewrite kp/ki/kd
 * at runtime — e.g. from a set-point tag pushed by the hub.
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#ifndef OSODB_PID_H
#define OSODB_PID_H

typedef struct {
    double kp, ki, kd;       /* gains (tunable at runtime) */
    double out_min, out_max; /* output clamp */
    double integ;            /* integral accumulator (with anti-windup) */
    double prev_err;         /* for the derivative term */
    int    primed;           /* 0 until the first step seeds prev_err */
} pid_t;

void   pid_init(pid_t *p, double kp, double ki, double kd, double out_min, double out_max);
void   pid_reset(pid_t *p);
/* one control step: setpoint, measurement, dt in seconds -> clamped output */
double pid_step(pid_t *p, double setpoint, double measured, double dt);

#endif /* OSODB_PID_H */
