gdb1: MODULE

SYNMODE m_arr1 = ARRAY (1:10) UBYTE;
SYNMODE m_struct = STRUCT ( i LONG,
                            p REF m_arr1);
SYNMODE m_arr2 = ARRAY (0:10) REF m_struct;

DCL v_arr1 m_arr1 INIT := [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
DCL v_struct m_struct INIT := [ 10, ->v_arr1 ];
DCL v_arr2 m_arr2 INIT := [ (5): ->v_struct, (ELSE): NULL ];

WRITETEXT (stdout, "v_arr2(5)->.p->(5) = %C%/", v_arr2(5)->.p->(5));
END gdb1;
