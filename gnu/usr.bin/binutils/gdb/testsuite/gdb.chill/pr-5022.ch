PR_5022: MODULE
  dummy_pr_5022: PROC ();
  END;
  DCL p PTR;
  DCL i INT;

  p := NULL;
  dummy_pr_5022 ();
  i := 13;
  p := ->i;
  dummy_pr_5022 ();
END;
