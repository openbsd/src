/*	$NetBSD: tc_3000_300.c,v 1.3 1995/08/03 00:52:29 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/pte.h>

#include <alpha/tc/tc.h>
#include <alpha/tc/tc_3000_300.h>

/* XXX ESTABLISH, DISESTABLISH */
void	tc_3000_300_intr_setup __P((void));
void	tc_3000_300_intr_establish
	    __P((struct confargs *, intr_handler_t, void *));
void	tc_3000_300_intr_disestablish __P((struct confargs *));
void	tc_3000_300_iointr __P((void *, int));
int	tc_3000_300_getdev __P((struct confargs *));

#define	KV(x)	((caddr_t)phystok0seg(x))
#define	TC_3000_300_NSLOTS	5
#define	TC_3000_300_MAXDEVS	5

static struct tc_slot_desc dec_3000_300_slots[TC_3000_300_NSLOTS] = {
	{ KV(0x100000000), },		/* slot 0 - TC option slot 0 */
	{ KV(0x120000000), },		/* slot 1 - TC option slot 1 */
	{ KV(0x180000000), },		/* slot 2 - TCDS ASIC on cpu board */
	{ KV(0x1a0000000), },		/* slot 3 - IOCTL ASIC on cpu board */
	{ KV(0x1c0000000), },		/* slot 4 - CXTurbo on cpu board */
};

static struct confargs dec_3000_300_devs[TC_3000_300_MAXDEVS] = {
	{ "PMAGB-BA",	4, 0x02000000,	},
	{ "IOCTL   ",	3, 0x00000000,	},
	{ "PMAZ-DS ",	2, 0x00000000,	},
	{ NULL,		1, 0x0,		},
	{ NULL,		0, 0x0,		},
};

/* Indices into the struct confargs array. */
#define	TC_3000_300_DEV_CXTURBO	0
#define	TC_3000_300_DEV_IOCTL	1
#define	TC_3000_300_DEV_TCDS	2
#define	TC_3000_300_DEV_OPT1	3
#define	TC_3000_300_DEV_OPT0	4

struct tc_cpu_desc dec_3000_300_cpu = {
	dec_3000_300_slots, TC_3000_300_NSLOTS,
	dec_3000_300_devs, TC_3000_300_MAXDEVS,
	tc_3000_300_intr_setup,
	tc_3000_300_intr_establish,
	tc_3000_300_intr_disestablish,
	tc_3000_300_iointr,
};

intr_handler_t	tc_3000_300_intrhand[TC_3000_300_MAXDEVS];
void		*tc_3000_300_intrval[TC_3000_300_MAXDEVS];

void
tc_3000_300_intr_setup()
{
	int i;

	/* Set up interrupt handlers. */
	for (i = 0; i < TC_3000_300_MAXDEVS; i++) {
		tc_3000_300_intrhand[i] = tc_intrnull;
		tc_3000_300_intrval[i] = (void *)(long)i;
	}
}

void
tc_3000_300_intr_establish(ca, handler, val)
	struct confargs *ca;
	int (*handler) __P((void *));
	void *val;
{
	int dev = tc_3000_300_getdev(ca);

#ifdef DIAGNOSTIC
	if (dev == -1)
		panic("tc_3000_300_intr_establish: dev == -1");
#endif

	if (tc_3000_300_intrhand[dev] != tc_intrnull)
		panic("tc_3000_300_intr_establish: dev %d twice", dev);

	tc_3000_300_intrhand[dev] = handler;
	tc_3000_300_intrval[dev] = val;

	/* XXX ENABLE INTERRUPT MASK FOR DEV */
}

void
tc_3000_300_intr_disestablish(ca)
	struct confargs *ca;
{
	int dev = tc_3000_300_getdev(ca);

#ifdef DIAGNOSTIC
	if (dev == -1)
		panic("tc_3000_300_intr_disestablish: somebody goofed");
#endif

	if (tc_3000_300_intrhand[dev] == tc_intrnull)
		panic("tc_3000_300_intr_disestablish: dev %d missing intr",
		    dev);

	tc_3000_300_intrhand[dev] = tc_intrnull;
	tc_3000_300_intrval[dev] = (void *)(long)dev;

	/* XXX DISABLE INTERRUPT MASK FOR DEV */
}

void
tc_3000_300_iointr(framep, vec)
	void *framep;
	int vec;
{
	u_int32_t ir;
	int ifound;

#ifdef DIAGNOSTIC
	int s;
	if (vec != 0x800)
		panic("INVALID ASSUMPTION: vec %x, not 0x800", vec);
	s = splhigh();
	if (s != PSL_IPL_IO)
		panic("INVALID ASSUMPTION: IPL %d, not %d", s, PSL_IPL_IO);
	splx(s);
#endif

	do {
		MAGIC_READ;
		wbflush();

		/* find out what interrupts/errors occurred */
		ir = *(volatile u_int32_t *)TC_3000_300_IR;
		wbflush();

		/* clear the interrupts/errors we found. */
		*(volatile u_int32_t *)TC_3000_300_IR = ir;
		wbflush();

		ifound = 0;
#define	CHECKINTR(slot, bits)						\
		if (ir & bits) {					\
			ifound = 1;					\
			(*tc_3000_300_intrhand[slot])			\
			    (tc_3000_300_intrval[slot]);		\
		}
		/* Do them in order of priority; highest slot # first. */
		CHECKINTR(TC_3000_300_DEV_CXTURBO, TC_3000_300_IR_CXTURBO);
		CHECKINTR(TC_3000_300_DEV_IOCTL, TC_3000_300_IR_IOCTL);
		CHECKINTR(TC_3000_300_DEV_TCDS, TC_3000_300_IR_TCDS);
#if 0
		CHECKINTR(TC_3000_300_DEV_OPT1, TC_3000_300_IR_OPT1);
		CHECKINTR(TC_3000_300_DEV_OPT0, TC_3000_300_IR_OPT0);
#else
		/* XXX XXX XXX CHECK OPTION SLOT INTERRUPTS!!! */
		/* XXX XXX XXX THEIR BITS LIVE IN ANOTHER REG. */
#endif
#undef CHECKINTR


#ifdef DIAGNOSTIC
#define PRINTINTR(msg, bits)						\
	if (ir & bits)							\
		printf(msg);
		PRINTINTR("BCache tag parity error\n",
		    TC_3000_300_IR_BCTAGPARITY);
		PRINTINTR("TC overrun error\n", TC_3000_300_IR_TCOVERRUN);
		PRINTINTR("TC I/O timeout\n", TC_3000_300_IR_TCTIMEOUT);
		PRINTINTR("Bcache parity error\n",
		    TC_3000_300_IR_BCACHEPARITY);
		PRINTINTR("Memory parity error\n", TC_3000_300_IR_MEMPARITY);
#undef PRINTINTR
#endif
	} while (ifound);
}

int
tc_3000_300_getdev(ca)
	struct confargs *ca;
{
	int i;

	for (i = 0; i < TC_3000_300_MAXDEVS; i++)
		if (ca->ca_slot == dec_3000_300_devs[i].ca_slot &&
		    ca->ca_offset == dec_3000_300_devs[i].ca_offset &&
		    !strncmp(ca->ca_name, dec_3000_300_devs[i].ca_name))
			return (i);

	return (-1);
}
