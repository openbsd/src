xx: MODULE

DCL v_bool BOOL INIT := FALSE;
DCL v_char CHAR INIT := 'X';
DCL v_byte BYTE INIT := -30;
DCL v_ubyte UBYTE INIT := 30;
DCL v_int INT INIT := -333;
DCL v_uint UINT INIT := 333;
DCL v_long LONG INIT := -4444;
DCL v_ulong ULONG INIT := 4444;
DCL v_ptr PTR;

SYNMODE m_set = SET (e1, e2, e3, e4, e5, e6);
DCL v_set m_set INIT := e3;

SYNMODE m_set_range = m_set(e2:e5);
DCL v_set_range m_set_range INIT := e3;

SYNMODE m_numbered_set = SET (n1 = 25, n2 = 22, n3 = 35, n4 = 33,
			      n5 = 45, n6 = 43);
DCL v_numbered_set m_numbered_set INIT := n3;

SYNMODE m_char_range = CHAR('A':'Z');
DCL v_char_range m_char_range INIT := 'G';

SYNMODE m_bool_range = BOOL(FALSE:FALSE);
DCL v_bool_range m_bool_range;

SYNMODE m_long_range = LONG(255:3211);
DCL v_long_range m_long_range INIT := 1000;

SYNMODE m_range = RANGE(12:28);
DCL v_range m_range INIT := 23;

SYNMODE m_chars = CHARS(20);
SYNMODE m_chars_v = CHARS(20) VARYING;
DCL v_chars CHARS(20);
DCL v_chars_v CHARS(20) VARYING INIT := "foo bar";

SYNMODE m_bits = BOOLS(10);
DCL v_bits BOOLS(10);

SYNMODE m_arr = ARRAY(1:10) BYTE;
DCL v_arr ARRAY(1:10) BYTE;

SYNMODE m_char_arr = ARRAY (CHAR) BYTE;
DCL v_char_arr ARRAY(CHAR) BYTE;

SYNMODE m_bool_arr = ARRAY (BOOL) BYTE;
DCL v_bool_arr ARRAY (BOOL) BYTE;

SYNMODE m_int_arr = ARRAY (INT) BYTE;
DCL v_int_arr ARRAY (INT) BYTE;

SYNMODE m_set_arr = ARRAY (m_set) BYTE;
DCL v_set_arr ARRAY (m_set) BYTE;

SYNMODE m_numbered_set_arr = ARRAY (m_numbered_set) BYTE;
DCL v_numbered_set_arr ARRAY (m_numbered_set) BYTE;

SYNMODE m_char_range_arr = ARRAY (m_char_range) BYTE;
DCL v_char_range_arr ARRAY (m_char_range) BYTE;

SYNMODE m_set_range_arr = ARRAY (m_set_range) BYTE;
DCL v_set_range_arr ARRAY (m_set_range) BYTE;

SYNMODE m_bool_range_arr = ARRAY (m_bool_range) BYTE;
DCL v_bool_range_arr ARRAY (m_bool_range) BYTE;

SYNMODE m_long_range_arr = ARRAY (m_long_range) BYTE;
DCL v_long_range_arr ARRAY (m_long_range) BYTE;

SYNMODE m_range_arr = ARRAY (m_range) BYTE;
DCL v_range_arr ARRAY (m_range) BYTE;

SYNMODE m_struct = STRUCT (i LONG,
                           c CHAR,
                           s CHARS(30));
DCL v_struct m_struct;

v_bool := TRUE;

END xx;
