VAR_GLOBAL
  a : ARRAY[0..4] OF DINT;
  i : DINT := 0;
  sum : DINT := 0;
END_VAR

PROCEDURE main()
  FOR i := 0 TO 4 DO
    a[i] := i * 10;
  END_FOR;
  sum := 0;
  FOR i := 0 TO 4 DO
    sum := sum + a[i];
  END_FOR;
  debug(sum);
  debug(a[2]);
END_PROCEDURE
