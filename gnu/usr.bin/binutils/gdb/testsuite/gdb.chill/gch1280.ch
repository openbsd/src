gch1280: MODULE

SYNMODE m_x = ARRAY (1:3) LONG;
DCL v_x m_x;
DCL v_xx m_x;

doit: PROC ()
END doit;

v_x := [ 11, 12, 13 ];
doit ();

END gch1280;
