VAR_GLOBAL
  msg : STRING := 'hello';
END_VAR

PROCEDURE main()
  debug(msg);
  debug('world');
END_PROCEDURE
