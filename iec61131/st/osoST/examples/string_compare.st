VAR_GLOBAL
  msg : STRING := 'on';
  matched : DINT := 0;
  other : DINT := 0;
END_VAR

PROCEDURE main()
  IF msg = 'on' THEN
    matched := 1;
  END_IF;
  IF msg = 'off' THEN
    other := 1;
  END_IF;
  debug(matched);
  debug(other);
END_PROCEDURE
