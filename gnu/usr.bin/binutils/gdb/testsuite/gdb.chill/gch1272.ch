gch1272: MODULE

SYNMODE m_array = ARRAY (0:99) INT;
DCL foo m_array;

SYNMODE m_xxx = ARRAY (1:10) LONG;

SYNMODE m_struct = STRUCT (i LONG, b BOOL);
SYNMODE m_bar = ARRAY (-10:20) m_struct;
DCL bar m_bar;

SYNMODE m_ps = POWERSET LONG (0:20);

brrr: PROC ()
END;

foo := [ (*): 222 ];

brrr ();

END gch1272;
