/*	$NetBSD: tc_3000_500.c,v 1.2 1995/08/03 00:52:36 cgd Exp $	*/

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
#include <alpha/tc/tc_3000_500.h>

/* XXX ESTABLISH, DISESTABLISH */
void	tc_3000_500_intr_setup __P((void));
void	tc_3000_500_intr_establish
	    __P((struct confargs *, intr_handler_t, void *));
void	tc_3000_500_intr_disestablish __P((struct confargs *));
void	tc_3000_500_iointr __P((void *, int));
int	tc_3000_500_getdev __P((struct confargs *));

#define	KV(x)	((caddr_t)phystok0seg(x))
#define	TC_3000_500_NSLOTS	8
#define	TC_3000_500_MAXDEVS	9

static struct tc_slot_desc dec_3000_500_slots[TC_3000_500_NSLOTS] = {
	{ KV(0x100000000), },		/* slot 0 - TC option slot 0 */
	{ KV(0x120000000), },		/* slot 1 - TC option slot 1 */
	{ KV(0x140000000), },		/* slot 2 - TC option slot 2 */
	{ KV(0x160000000), },		/* slot 3 - TC option slot 3 */
	{ KV(0x180000000), },		/* slot 4 - TC option slot 4 */
	{ KV(0x1a0000000), },		/* slot 5 - TC option slot 5 */
	{ KV(0x1c0000000), },		/* slot 6 - TCDS ASIC on cpu board */
	{ KV(0x1e0000000), },		/* slot 7 - IOCTL ASIC on cpu board */
};

static struct confargs dec_3000_500_devs[TC_3000_500_MAXDEVS] = {
	{ "IOCTL   ",	7, 0x00000000,	},
	{ "PMAGB-BA",	7, 0x02000000,	},
	{ "PMAZ-DS ",	6, 0x00000000,	},
	{ NULL,		5, 0x0,		},
	{ NULL,		4, 0x0,		},
	{ NULL,		3, 0x0,		},
	{ NULL,		2, 0x0,		},
	{ NULL,		1, 0x0,		},
	{ NULL,		0, 0x0,		},
};

/* Indices into the struct confargs array. */
#define	TC_3000_500_DEV_IOCTL	0
#define	TC_3000_500_DEV_CXTURBO	1
#define	TC_3000_500_DEV_TCDS	2
#define	TC_3000_500_DEV_OPT5	3
#define	TC_3000_500_DEV_OPT4	4
#define	TC_3000_500_DEV_OPT3	5
#define	TC_3000_500_DEV_OPT2	6
#define	TC_3000_500_DEV_OPT1	7
#define	TC_3000_500_DEV_OPT0	8

struct tc_cpu_desc dec_3000_500_cpu = {
	dec_3000_500_slots, TC_3000_500_NSLOTS,
	dec_3000_500_devs, TC_3000_500_MAXDEVS,
	tc_3000_500_intr_setup,
	tc_3000_500_intr_establish,
	tc_3000_500_intr_disestablish,
	tc_3000_500_iointr,
};

intr_handler_t	tc_3000_500_intrhand[TC_3000_500_MAXDEVS];
void		*tc_3000_500_intrval[TC_3000_500_MAXDEVS];

void
tc_3000_500_intr_setup()
{
	int i;

        /* Set up interrupt handlers. */
        for (i = 0; i < TC_3000_500_MAXDEVS; i++) {
                tc_3000_500_intrhand[i] = tc_intrnull;
                tc_3000_500_intrval[i] = (void *)(long)i;
        }

	/*
	 * XXX
	 * The System Programmer's Manual (3-15) says IMR entries for option
	 * slots are initialized to 0.  I think this is wrong, and that they
	 * are initialized to 1, i.e. the option slots are disabled.  Enable
	 * them.
	 *
	 * XXX
	 * The MACH code appears to enable them by setting them to 1.  !?!?!
	 */
	*(volatile u_int32_t *)TC_3000_500_IMR_WRITE = 0;
	wbflush();
}

void
tc_3000_500_intr_establish(ca, handler, val)
	struct confargs *ca;
	int (*handler) __P((void *));
	void *val;
{
	int dev = tc_3000_500_getdev(ca);

#ifdef DIAGNOSTIC
	if (dev == -1)
		panic("tc_3000_500_intr_establish: dev == -1");
#endif

	if (tc_3000_500_intrhand[dev] != tc_intrnull)
		panic("tc_3000_500_intr_establish: dev %d twice", dev);

	tc_3000_500_intrhand[dev] = handler;
	tc_3000_500_intrval[dev] = val;

	/* XXX ENABLE INTERRUPT MASK FOR DEV */
}

void
tc_3000_500_intr_disestablish(ca)
	struct confargs *ca;
{
	int dev = tc_3000_500_getdev(ca);

#ifdef DIAGNOSTIC
	if (dev == -1)
		panic("tc_3000_500_intr_disestablish: somebody goofed");
#endif

	if (tc_3000_500_intrhand[dev] == tc_intrnull)
		panic("tc_3000_500_intr_disestablish: dev %d missing intr",
		    dev);

	tc_3000_500_intrhand[dev] = tc_intrnull;
	tc_3000_500_intrval[dev] = (void *)(long)dev;

	/* XXX DISABLE INTERRUPT MASK FOR DEV */
}

void
tc_3000_500_iointr(framep, vec)
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
		ir = *(volatile u_int32_t *)TC_3000_500_IR_CLEAR;
		wbflush();

		ifound = 0;
#define	CHECKINTR(slot, bits)						\
		if (ir & bits) {					\
			ifound = 1;					\
			(*tc_3000_500_intrhand[slot])			\
			    (tc_3000_500_intrval[slot]);		\
		}
		/* Do them in order of priority; highest slot # first. */
		CHECKINTR(TC_3000_500_DEV_CXTURBO, TC_3000_500_IR_CXTURBO);
		CHECKINTR(TC_3000_500_DEV_IOCTL, TC_3000_500_IR_IOCTL);
		CHECKINTR(TC_3000_500_DEV_TCDS, TC_3000_500_IR_TCDS);
		CHECKINTR(TC_3000_500_DEV_OPT5, TC_3000_500_IR_OPT5);
		CHECKINTR(TC_3000_500_DEV_OPT4, TC_3000_500_IR_OPT4);
		CHECKINTR(TC_3000_500_DEV_OPT3, TC_3000_500_IR_OPT3);
		CHECKINTR(TC_3000_500_DEV_OPT2, TC_3000_500_IR_OPT2);
		CHECKINTR(TC_3000_500_DEV_OPT1, TC_3000_500_IR_OPT1);
		CHECKINTR(TC_3000_500_DEV_OPT0, TC_3000_500_IR_OPT0);
#undef CHECKINTR

#ifdef DIAGNOSTIC
#define PRINTINTR(msg, bits)						\
	if (ir & bits)							\
		printf(msg);
		PRINTINTR("Second error occurred\n", TC_3000_500_IR_ERR2);
		PRINTINTR("DMA buffer error\n", TC_3000_500_IR_DMABE);
		PRINTINTR("DMA cross 2K boundary\n", TC_3000_500_IR_DMA2K);
		PRINTINTR("TC reset in progress\n", TC_3000_500_IR_TCRESET);
		PRINTINTR("TC parity error\n", TC_3000_500_IR_TCPAR);
		PRINTINTR("DMA tag error\n", TC_3000_500_IR_DMATAG);
		PRINTINTR("Single-bit error\n", TC_3000_500_IR_DMASBE);
		PRINTINTR("Double-bit error\n", TC_3000_500_IR_DMADBE);
		PRINTINTR("TC I/O timeout\n", TC_3000_500_IR_TCTIMEOUT);
		PRINTINTR("DMA block too long\n", TC_3000_500_IR_DMABLOCK);
		PRINTINTR("Invalid I/O address\n", TC_3000_500_IR_IOADDR);
		PRINTINTR("DMA scatter/gather invalid\n", TC_3000_500_IR_DMASG);
		PRINTINTR("Scatter/gather parity error\n",
		    TC_3000_500_IR_SGPAR);
#undef PRINTINTR
#endif
	} while (ifound);
}

int
tc_3000_500_getdev(ca)
	struct confargs *ca;
{
	int i;

	for (i = 0; i < TC_3000_500_MAXDEVS; i++)
		if (ca->ca_slot == dec_3000_500_devs[i].ca_slot &&
		    ca->ca_offset == dec_3000_500_devs[i].ca_offset &&
		    !strncmp(ca->ca_name, dec_3000_500_devs[i].ca_name))
			return (i);

	return (-1);
}

/*
 * tc_3000_500_ioslot --
 *	Set the PBS bits for devices on the TC.
 */
void
tc_3000_500_ioslot(slot, flags, set)
	u_int32_t slot, flags;
	int set;
{
	volatile u_int32_t *iosp;
	u_int32_t ios;
	int s;
	
	iosp = (volatile u_int32_t *)TC_3000_500_IOSLOT;
	ios = *iosp;
	flags <<= (slot * 3);
	if (set)
		ios |= flags;
	else
		ios &= ~flags;
	s = splhigh();
	*iosp = ios;
	wbflush();
	splx(s);
}
