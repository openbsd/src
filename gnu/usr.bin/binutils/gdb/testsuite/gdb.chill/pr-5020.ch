
PR_5020: MODULE
  dummy_pr_5020: PROC ();
  END;
  NEWMODE x = STRUCT (l LONG, b BOOL);
  NEWMODE aset = SET (aa, bb);

  DCL y ARRAY ('a':'b') x;
  DCL setarr ARRAY (aset) x;
  DCL intarr ARRAY(10:11) x;
  DCL boolarr ARRAY (BOOL) x;

  y('a').l, setarr(aa).l, intarr(10).l, boolarr(FALSE).l := 10;
  y('a').b, setarr(aa).b, intarr(10).b, boolarr(FALSE).b := TRUE;
  y('b').l, setarr(bb).l, intarr(11).l, boolarr(TRUE).l := 111;
  y('b').b, setarr(bb).b, intarr(11).b, boolarr(TRUE).b := FALSE;

  dummy_pr_5020 ();
END;
