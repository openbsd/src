xx: MODULE

SYNMODE m_set1 = SET (e1, e2, e3, e4, e5);
DCL v_set1 m_set1 INIT := e3;

SYNMODE m_set2 = SET (a1=1, a2=2, a3=17, a4=9, a5=8, a6=0, a7=14, a8=33, a9=12);
DCL v1_set2 m_set2 INIT := a1;
DCL v2_set2 m_set2 INIT := a2;
DCL v3_set2 m_set2 INIT := a3;
DCL v4_set2 m_set2 INIT := a4;
DCL v5_set2 m_set2 INIT := a5;
DCL v6_set2 m_set2 INIT := a6;
DCL v7_set2 m_set2 INIT := a7;
DCL v8_set2 m_set2 INIT := a8;
DCL v9_set2 m_set2 INIT := a9;

SYNMODE m_set3 = SET (b1, b2, b3, b4, b5 = 4711, b6, b7 = 4713);
DCL v_set3 m_set3 INIT := b7;

SYNMODE m_set4 = SET(s1=111111, s2, s3, s4); 
DCL v1_set4 m_set4 INIT := s1;

SYNMODE m_set_range = m_set1(e2:e5);
DCL v_set_range m_set_range INIT := e3;

SYNMODE m_set_range_arr = ARRAY (m_set_range) BYTE;
DCL v_set_range_arr ARRAY (m_set_range) BYTE;

SYNMODE m_set_arr = ARRAY (m_set1) BYTE;
DCL v_set_arr ARRAY (m_set1) BYTE;

NEWMODE m_power1 = POWERSET m_set1;
DCL v1_power1 READ m_power1 INIT := [e1,e2,e3,e4,e5];
DCL v2_power1 m_power1 INIT := [];

NEWMODE m_power2 = POWERSET m_set2;
DCL v_power2 m_power2 INIT := [];

NEWMODE m_power3 = POWERSET m_set3;
DCL v_power3 m_power3 INIT := [b1:b2];

NEWMODE m_power4 = POWERSET CHAR;
DCL v_power4 m_power4 INIT := ['b':'x'];

NEWMODE m_power5 = POWERSET INT (2:400);
DCL v_power5 m_power5 INIT := [2:100];

NEWMODE m_power6 = POWERSET INT;
DCL v_power6 m_power6;

NEWMODE m_power7 = POWERSET LONG;
DCL v_power7 m_power7;


v_set1:= e2;
v2_power1:= [e1];

v_set1:= e1;

END xx;
