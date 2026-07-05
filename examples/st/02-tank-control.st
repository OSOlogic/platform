(* ============================================================
   Tank control — Structured Text
   Pump ON when level drops below LOW, OFF when it rises above HIGH.
   Hysteresis avoids chattering around a single set-point.
   (level_pct / pump map to osodb tags via the project's I/O bindings.)
   ============================================================ *)
VAR_GLOBAL
  level_pct : DINT := 0;      (* process value 0..100 *)
  pump      : BOOL := FALSE;  (* output *)
  low       : DINT := 20;
  high      : DINT := 80;
END_VAR

PROCEDURE main()
  IF level_pct <= low THEN
    pump := TRUE;             (* refill *)
  ELSIF level_pct >= high THEN
    pump := FALSE;            (* full *)
  END_IF;                     (* between low..high: hold state (hysteresis) *)
  debug(level_pct);
  debug(pump);
END_PROCEDURE
