VAR_GLOBAL
  i : DINT := 7;
  x : REAL := 0.0;
  y : REAL := 0.0;
  z : DINT := 0;
END_VAR

PROCEDURE main()
  x := i * 2.5;
  y := TO_REAL(i) + 0.5;
  z := REAL_TO_INT(x);
  debug(x);
  debug(y);
  debug(z);
END_PROCEDURE
