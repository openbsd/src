ss: MODULE

/* These declarations are from Cygnus PR chill/9078. */
  SYNMODE m_char20 = CHARS(20) VARYING;

  DCL foo m_char20 INIT := "Moser ";
  DCL bar m_char20 INIT := "Wilfried";

  DCL foo1 CHARS(5) INIT := "12345";
  DCL bar1 CHARS(5) INIT := "abcde";

/* This is Cynus PR chill/5696. */

DCL s20 CHARS(20) VARYING;

DCL s10 CHARS(10);


s20 := "Moser Wilfried";
S10 := "1234567890";

WRITETEXT (stdout, "s20 := ""%C"", s10 := ""%C""%/", s20, s10);

END ss;
