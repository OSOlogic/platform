(* ============================================================
   osoLogic — IEC 61131-3 Structured Text example
   pid.st — Simple discrete PID controller

   Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
                 Ibercomp SL, Roig Borrell SL

   Part of the osoLogic open-source PLC project — osologic.org
   SPDX-License-Identifier: AGPL-3.0-or-later

   Description / Descripción:
     Position-form discrete PID (P + I + D terms).
     The scan cycle in osoruntime.c calls main() at a fixed period.
     Call pid_update() from your main procedure, passing the current
     process value and setpoint. Returns control output clamped to
     [out_min, out_max].

     PID discreto en forma posición (términos P + I + D).
     El ciclo de scan en osoruntime.c llama a main() a periodo fijo.
     Llame a pid_update() desde su procedimiento principal, pasando
     el valor de proceso actual y el setpoint. Devuelve la salida de
     control limitada a [out_min, out_max].

   Usage / Uso:
     output := pid_update(process_value, setpoint);
   ============================================================ *)

(* PID tuning parameters / Parámetros de sintonía PID *)
VAR_GLOBAL
  pid_kp      : REAL := 1.0;   (* Proportional gain / Ganancia proporcional *)
  pid_ki      : REAL := 0.1;   (* Integral gain     / Ganancia integral     *)
  pid_kd      : REAL := 0.05;  (* Derivative gain   / Ganancia derivativa   *)
  pid_out_min : REAL := -100.0;
  pid_out_max : REAL :=  100.0;
END_VAR

(* Internal state — do NOT modify from outside *)
(* Estado interno — NO modificar desde fuera   *)
VAR_GLOBAL
  _pid_integral : REAL := 0.0;
  _pid_prev_err : REAL := 0.0;
END_VAR

(* ── PID update function / Función de actualización PID ──── *)
FUNCTION pid_update : REAL
VAR_INPUT
  pv       : REAL;   (* Process value / Valor de proceso *)
  setpoint : REAL;   (* Desired value / Valor deseado    *)
END_VAR
VAR
  err    : REAL;
  p_term : REAL;
  d_term : REAL;
  output : REAL;
END_VAR
  err    := setpoint - pv;

  (* Proportional / Proporcional *)
  p_term := pid_kp * err;

  (* Integral with anti-windup clamp / Integral con límite anti-windup *)
  _pid_integral := _pid_integral + pid_ki * err;
  IF _pid_integral > pid_out_max THEN
    _pid_integral := pid_out_max;
  ELSIF _pid_integral < pid_out_min THEN
    _pid_integral := pid_out_min;
  END_IF;

  (* Derivative / Derivativo *)
  d_term := pid_kd * (err - _pid_prev_err);
  _pid_prev_err := err;

  (* Sum and clamp output / Suma y limitación de salida *)
  output := p_term + _pid_integral + d_term;
  IF output > pid_out_max THEN
    output := pid_out_max;
  ELSIF output < pid_out_min THEN
    output := pid_out_min;
  END_IF;

  pid_update := output;
END_FUNCTION


(* ── Demo main procedure / Procedimiento principal demo ──── *)
VAR_GLOBAL
  process_temp : REAL := 20.0;   (* Simulated sensor   / Sensor simulado   *)
  heater_out   : REAL := 0.0;    (* Heater power %     / Potencia calefactor % *)
END_VAR

PROCEDURE main()
VAR
  sp : REAL;
END_VAR
  sp          := 60.0;   (* Target temperature / Temperatura objetivo *)
  heater_out  := pid_update(process_temp, sp);
  (* Simulate plant response / Simular respuesta de planta *)
  process_temp := process_temp + 0.01 * heater_out - 0.005 * (process_temp - 20.0);
  debug(process_temp, heater_out);
END_PROCEDURE
