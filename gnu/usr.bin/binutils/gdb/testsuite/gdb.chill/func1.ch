func1: MODULE

SYNMODE m_set = SET (e1, e2, e3, e4, e5, e6, e7, e8, e9, e10);
SYNMODE m_setrange = RANGE (e3:e8);
SYNMODE m_ps = POWERSET m_set;
SYNMODE m_rangeps = POWERSET RANGE(0:31);
GRANT ALL;

END func1;
