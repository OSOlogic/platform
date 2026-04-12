(* ============================================================
   osoLogic — IEC 61131-3 Structured Text example
   counter.st — Up/down counter with debug output

   Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
                 Ibercomp SL, Roig Borrell SL

   Part of the osoLogic open-source PLC project — osologic.org
   SPDX-License-Identifier: AGPL-3.0-or-later

   Description / Descripción:
     Simple up counter incremented each scan cycle.
     Demonstrates: global variables, IF, debug output.
     Contador ascendente incrementado cada ciclo de scan.
     Demuestra: variables globales, IF, salida debug.
   ============================================================ *)

VAR_GLOBAL
  count    : DINT  := 0;
  max_val  : DINT  := 100;
  overflow : BOOL  := FALSE;
END_VAR

PROCEDURE main()
  count := count + 1;
  IF count >= max_val THEN
    count    := 0;
    overflow := TRUE;
    debug("Counter overflow / Desbordamiento del contador");
  ELSE
    overflow := FALSE;
  END_IF;
  debug(count);
END_PROCEDURE
