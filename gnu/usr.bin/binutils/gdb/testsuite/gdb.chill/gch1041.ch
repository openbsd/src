arr: MODULE

SYNMODE m_chars = CHARS(30) VARYING;
SYNMODE m_s = STRUCT (l LONG, c m_chars, b BOOL);

DCL a1 ARRAY (1:1000) LONG INIT := [(5:100): 33, (1:4): 44, (ELSE): 55 ];
DCL a2 ARRAY (1:10) m_s INIT := [(*): [ 22, "mowi", TRUE ] ];
DCL a3 ARRAY (CHAR) CHAR INIT := [(*): 'X'];

SYNMODE m_set = SET (e1, e2, e3, e4, e5, e6, e7, e9, e10);
DCL a4 ARRAY (m_set) BOOL INIT := [(*): TRUE];

a1 := [(5:100): 33, (1:4): 44, (ELSE): 55 ];
a1 := [ (*): 22 ];
a2 := [(*): [ 22, "mowi", TRUE ] ];

END arr;
