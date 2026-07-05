VAR_GLOBAL
  r : ARRAY[1..3] OF REAL;
  s : REAL := 0.0;
END_VAR

PROCEDURE main()
  r[1] := 1.5;
  r[2] := r[1] * 2.0;
  r[3] := r[2] + r[1];
  s := r[1] + r[2] + r[3];
  debug(s);
  debug(r[3]);
END_PROCEDURE
