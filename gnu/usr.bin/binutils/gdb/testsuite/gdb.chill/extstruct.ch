pottendo: MODULE

<> USE_SEIZE_FILE "extstruct-grt.grt" <>
SEIZE m_array3;
SEIZE m_array4;

SYNMODE m_x = STRUCT (i long,
		      ar m_array3);
SYNMODE m_y = STRUCT (i long,
		      ar m_array4);

DCL x LONG;

x := 10;

END pottendo;
