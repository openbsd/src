test_result:  MODULE

  DCL i INT := 5;

  SYNMODE m_struct = STRUCT (l LONG, b BOOL);
  DCL v_struct m_struct := [ 20, TRUE ];

  simple_func: PROC () RETURNS (INT);
    DCL j INT := i;
    RESULT 10;
    i + := 2;
    RESULT j + 2;
    i + := 2;
  END simple_func;

  ret_struct: PROC () RETURNS (m_struct)
    DCL v m_struct := [ 33, FALSE ];
    RESULT v;
    v.l := 18;
  END ret_struct;

  i := simple_func ();
  i := simple_func ();
  i * := 10;

  v_struct := ret_struct ();

  i := 33; -- for gdb
END test_result;
