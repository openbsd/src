--
-- check powerset operators and built-ins
--

ps: MODULE

SYNMODE m_ps1 = POWERSET ULONG (0:8);
DCL v_ps1 m_ps1 INIT := [1,3,5,7];

SYNMODE m_ps2 = POWERSET LONG (-100:100);
DCL v_ps2 m_ps2 INIT := [ -100:-95, -1:1, 95:100];

SYNMODE m_set = SET (aa, bb, cc, dd, ee, ff, gg, hh, ii, jj);
SYNMODE m_ps3 = POWERSET m_set;
DCL v_ps3 m_ps3 INIT := [bb, dd, ff, ii];

SYNMODE m_ps4 = POWERSET CHAR(' ':'z');
DCL v_ps4 m_ps4 INIT := [ '.', ',', 'A':'F', 'x':'z' ];

SYNMODE m_ps5 = POWERSET BOOL;
DCL v_ps5 m_ps5 INIT := [ FALSE ];
DCL v_ps51 m_ps5 INIT := [ ];

SYNMODE m_int_range = INT(-100:100);
SYNMODE m_int_subrange = m_int_range(-50:50);
SYNMODE m_ps6 = POWERSET m_int_subrange;
DCL v_ps6 m_ps6 INIT := [ LOWER(m_int_subrange):UPPER(m_int_subrange) ];

DCL x INT;

x := 25;

END ps;
