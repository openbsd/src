/*	$NetBSD: ds-asic-conf.c,v 1.4 1996/01/03 20:39:14 jonathan Exp $	*/

/*
 * Copyright (c) 1995 Jonathan Stone
 * All rights reserved.
 *
 * DECstation IO ASIC  subslot configuration
 */

#if 0
struct asic_slot {
	struct confargs	as_ca;
	u_int	as_bits;
	intr_handler_t	as_handler;
	void		*as_val;
};
#endif

/* Initial handler must be asic_intrnull or the ASIC newconf code panics */

struct asic_slot kn03_asic_slots[] =
{
		/* name        slot  offset intpri  */
	{ { "lance",	  0, (u_int) (3 * 0x40000), KN03_LANCE_SLOT, },
	    KN03_INTR_LANCE, asic_intrnull, (void*) KN03_LANCE_SLOT, },

	{ { "scc",	  1, (u_int) (4 * 0x40000), KN03_SCC0_SLOT, },
	    KN03_INTR_SCC_0, asic_intrnull, (void *)KN03_SCC0_SLOT, },

	{ { "scc",	  2, (u_int) (6 * 0x40000), KN03_SCC1_SLOT, },
	    KN03_INTR_SCC_1, asic_intrnull, (void *)KN03_SCC1_SLOT, },

	{ { "dallas_rtc", 3, (u_int) (8* 0x40000),  0 /*XXX*/, },
	    0,               asic_intrnull, (void *)(long) 16 /*XXX*/, },

	{ { "asc",	  4, (u_int) (12* 0x40000), KN03_SCSI_SLOT, },
	    0,               asic_intrnull, (void *)KN03_SCSI_SLOT, },

	{ { NULL, 0, 0, 0 }, 0, NULL, NULL }
};


struct asic_slot xine_asic_slots[] =
{
	{ { "lance",	   0, (u_int) (3 * 0x40000), KN03_LANCE_SLOT, },
	    KN03_INTR_LANCE, asic_intrnull, (void*) KN03_LANCE_SLOT, },

	{ { "scc",	   1, (u_int) (4 * 0x40000),  KN03_SCC0_SLOT, },
	    KN03_INTR_SCC_0, asic_intrnull, (void *)KN03_SCC0_SLOT, },

	{ { "dallas_rtc",  2, 0, (u_int) (8* 0x40000), },
	    0, asic_intrnull, (void *)(long) 16 /*XXX*/, },

	{ { "isdn",	   3, (u_int) (9 * 0x40000), XINE_ISDN_SLOT, },
	    0,		      asic_intrnull, (void *)(long) XINE_ISDN_SLOT, },

	{ { "dtop",	   4, (u_int) (10* 0x40000), XINE_DTOP_SLOT, },
	    0,		      asic_intrnull, (void *)(long) XINE_DTOP_SLOT, },

	{ { "fdc",	   5, (u_int) (11* 0x40000), XINE_FLOPPY_SLOT, },
	    0,		     asic_intrnull, (void *) (long)XINE_FLOPPY_SLOT, },

	{ { "asc",	   6, (u_int) (12* 0x40000), XINE_SCSI_SLOT, },
	    0 /*XINE_INTR_SCSI*/, asic_intrnull, (void*)XINE_SCSI_SLOT, },
#if 0
	{ { "frc",	   3, (u_int) (15* 0x40000), XINE_SLOT_FRC, },
	    0,		      asic_intrnull, (void *)(long) XINE_SLOT_FRC, },
#endif
	{ { NULL, 0, 0, }, 0, NULL, NULL }
};

/*
 * The 3MAX (KN02) doesn't even have an asic but for now,
 * configure its system slot as if it did.
 * Instead there's a 4 Mbyte "system" slot with eight 512 Kbyte subslots
 * for system devices:
 * 0=ROM, 1=(reserved), 2=CHKSYN, 3=ERRADDR, 4=DZ, 5=CLOCK, 6=CSR, 7=ROM1
 * These are mapped onto slot numbers as
 * tc0=1, tc1=1, tc2=2, unsed=3, unused=4, scsi=5, ether=6, dc=7
 */

struct asic_slot kn02_asic_slots[] = {
	{ { "dc",	   0,  (u_int) (4 * 0x80000), 7 },
	    KN03_INTR_SCC_0, asic_intrnull, (void *) 7, },
	
	{ { "dallas_rtc",  0, (u_int) (5 * 0x80000), 0, },
	    0, 		    asic_intrnull, (void *) 16 /*XXX*/, },

	{ { NULL, 0, 0 },  0, NULL, NULL }
};


