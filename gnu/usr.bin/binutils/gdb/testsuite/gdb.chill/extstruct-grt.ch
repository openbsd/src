pot1: MODULE

SYNMODE m_array1 = ARRAY (2:3) ulong;
SYNMODE m_struct = STRUCT (f1 int,
			   f2 REF m_array1,
			   f3 m_array1);
SYNMODE m_array3 = ARRAY (5:6) m_struct;
SYNMODE m_array4 = ARRAY (7:8) ARRAY (9:10) m_struct;

GRANT all;

END pot1;
