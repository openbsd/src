-- NOTE: This test is used for pr-3134.exp as well as pr-8136.
func: MODULE

<> USE_SEIZE_FILE "func1.grt" <>
SEIZE ALL;

NEWMODE m_struct = STRUCT (i LONG, str CHARS(50) VARYING);
DCL insarr ARRAY (1:10) INT;

DCL setrange m_setrange := e5;

DCL ps m_ps := [ e3, e7:e9 ];
DCL range_ps m_rangeps := [ 2, 3, 4, 28 ];

p1: PROC (first INT IN, last INT IN, s m_struct IN);

  DCL foo LONG := 3;

  startall: PROC ()
    DO FOR i := first to last;
      insarr(i) := i;
    OD;
    DO FOR i := first TO last;
      WRITETEXT (stdout, "insarr(%C) := %C%/", i, insarr(i));
    OD;
  END startall;

  startall ();

END p1;

p1 (LOWER (insarr), UPPER (insarr), [ 10, "This is a string." ]);

END func;
