markus1: MODULE

SYNMODE m_dummy = SET (dummy_1,
                       dummy_2,
                       dummy_3,
                       dummy_4,
                       dummy_5,
                       dummy_6,
                       dummy_7,
                       dummy_8,
                       dummy_9,
                       dummy_10,
                       dummy_11,
                       dummy_12,
                       dummy_13,
                       dummy_14,
                       dummy_15,
                       dummy_16,
                       dummy_17,
                       dummy_18,
                       dummy_19,
                       dummy_20,
                       dummy_21,
                       dummy_22,
                       dummy_23,
                       dummy_24,
                       dummy_25,
                       dummy_26);

SYNMODE m_dummy_range = m_dummy(dummy_6 : dummy_22);

GRANT m_dummy, m_dummy_range;

END markus1;
