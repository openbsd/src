emptybit: MODULE

SYNMODE b8 = BOOLS(8);
SYN bit8 b8 = B'00000000';

SYNMODE char_m = CHARS(40) VARYING;

SYNMODE stru_m = STRUCT (c char_m, b b8, boo BOOL);
DCL xx stru_m;

SYNMODE m_stru = STRUCT (c char_m, i LONG, boo BOOL);
DCL yy m_stru;

SYNMODE m_arr = ARRAY (1:10) LONG;
DCL zz m_arr;

WRITETEXT (stdout, "%C%/", bit8);

END emptybit;
