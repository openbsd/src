vector: MODULE

SYNMODE m_index = RANGE(1:10);
NEWMODE vector = ARRAY (m_index) INT;

DCL a, b, c vector;

dump: PROC( a vector LOC, c CHAR );
  DCL i m_index := 5;
  DO FOR i IN m_index;
    WRITETEXT( STDOUT, "%C(%C)=%C ", c, i, a(i) );
  OD;
  WRITETEXT( STDOUT, "%/" );
END dump;

a := vector [ 1, -1, 2, -2, 3, -3, 4, -4, 5, -5 ];
b := a;
b(4) := 4;
b(7) := 7;
c := vector [(*): 0];

dump(a,'a');

END vector;
