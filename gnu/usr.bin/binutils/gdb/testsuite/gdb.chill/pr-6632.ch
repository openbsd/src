markus: MODULE

<> USE_SEIZE_FILE "pr-6632-grt.grt" <>
SEIZE m_dummy, m_dummy_range;

DCL v m_dummy_range;

NEWMODE is_str_descr = STRUCT (p PTR,
                               l INT,
                               flag STRUCT (x UBYTE,
                                            y SET (aa, bb, cc, dd, ee, ff)));
DCL des is_str_descr;

NEWMODE is_cb_debug = STRUCT (i INT,
                              channel m_dummy_range,
                              p PTR);
NEWMODE is_cb_debug_array = ARRAY (0:20) is_cb_debug;
DCL cb_debug is_cb_debug_array;
DCL cb_debug_index INT := 0;

p: PROC (pp is_str_descr IN, x m_dummy_range IN)
  DO WITH cb_debug(cb_debug_index);
    channel := x;
  OD;
END p;

p (des, dummy_10);
WRITETEXT (stdout, "cb_debug(%C).channel := %C%/", 
           cb_debug_index, cb_debug(cb_debug_index).channel);

END markus;
