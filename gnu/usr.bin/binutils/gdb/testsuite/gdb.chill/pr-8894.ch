xx: MODULE

<> USE_SEIZE_FILE "pr-8894-grt.grt" <>
SEIZE m_byte;

SYNMODE m_struct = STRUCT (a, b, c m_byte);

DCL v m_struct;

v.a := 100;

END xx;
