(* ============================================================
   Blink LED — Structured Text (osoST dialect)
   Toggles a boolean every 500 ms using the millis() hardware trap.
   The classic "Hello World" of automation.
   ============================================================ *)
TRAP millis() : DINT   TRAP #12;

VAR_GLOBAL
  led  : BOOL := FALSE;
  t0   : DINT := 0;
END_VAR

PROCEDURE main()
  IF (millis() - t0) >= 500 THEN
    led := NOT led;        (* flip the state *)
    t0  := millis();       (* remember when *)
  END_IF;
  debug(led);              (* watch it blink *)
END_PROCEDURE
