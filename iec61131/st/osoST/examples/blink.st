(* ============================================================
   osoLogic — IEC 61131-3 Structured Text example
   blink.st — LED blink using a counter and trap calls

   Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
                 Ibercomp SL, Roig Borrell SL

   Part of the osoLogic open-source PLC project — osologic.org
   SPDX-License-Identifier: AGPL-3.0-or-later

   Description / Descripción:
     Toggle a GPIO output every N scan cycles.
     Alterna una salida GPIO cada N ciclos de scan.

   Traps used / Traps utilizados:
     gpio_write(pin, value)  — trap #10
     millis()                — trap #12  (returns I32 milliseconds)
   ============================================================ *)

(* Hardware traps — link to hardware_linux.c or hardware_bare.c
   Traps de hardware — enlazar con hardware_linux.c o hardware_bare.c *)
TRAP gpio_write(pin : DINT; value : DINT) TRAP #10;
TRAP gpio_read(pin : DINT) : DINT        TRAP #11;
TRAP millis() : DINT                     TRAP #12;

(* ── Global state / Estado global ──────────────────────── *)
VAR_GLOBAL
  led_state    : BOOL  := FALSE;
  last_toggle  : DINT  := 0;
END_VAR

(* ── Constants / Constantes ─────────────────────────────── *)
VAR_GLOBAL
  LED_PIN      : DINT  := 0;   (* GPIO pin number / Número de pin GPIO *)
  BLINK_MS     : DINT  := 500; (* Toggle interval ms / Intervalo de alternancia ms *)
END_VAR

(* ── Main scan procedure / Procedimiento principal de scan ─ *)
PROCEDURE main()
VAR
  now : DINT;
END_VAR
  now := millis();
  IF (now - last_toggle) >= BLINK_MS THEN
    led_state   := NOT led_state;
    last_toggle := now;
    gpio_write(LED_PIN, led_state ? 1 : 0);
  END_IF;
END_PROCEDURE
