/*	$NetBSD: tc_3000_500.c,v 1.3 1995/12/20 00:43:30 cgd Exp $	*/

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

#include <dev/tc/tcvar.h>
#include <alpha/tc/tc_conf.h>
#include <alpha/tc/tc_3000_500.h>

void	tc_3000_500_intr_setup __P((void));
void	tc_3000_500_intr_establish __P((struct device *, void *,
	    tc_intrlevel_t, int (*)(void *), void *));
void	tc_3000_500_intr_disestablish __P((struct device *, void *));
void	tc_3000_500_iointr __P((void *, int));

int	tc_3000_500_intrnull __P((void *));

#define C(x)	((void *)(u_long)x)
#define	KV(x)	(phystok0seg(x))

struct tc_slotdesc tc_3000_500_slots[] = {
	{ KV(0x100000000), C(TC_3000_500_DEV_OPT0), },	/* 0 - opt slot 0 */
	{ KV(0x120000000), C(TC_3000_500_DEV_OPT1), },	/* 1 - opt slot 1 */
	{ KV(0x140000000), C(TC_3000_500_DEV_OPT2), },	/* 2 - opt slot 2 */
	{ KV(0x160000000), C(TC_3000_500_DEV_OPT3), },	/* 3 - opt slot 3 */
	{ KV(0x180000000), C(TC_3000_500_DEV_OPT4), },	/* 4 - opt slot 4 */
	{ KV(0x1a0000000), C(TC_3000_500_DEV_OPT5), },	/* 5 - opt slot 5 */
	{ KV(0x1c0000000), C(TC_3000_500_DEV_BOGUS), },	/* 6 - TCDS ASIC */
	{ KV(0x1e0000000), C(TC_3000_500_DEV_BOGUS), },	/* 7 - IOCTL ASIC */
};
int tc_3000_500_nslots =
    sizeof(tc_3000_500_slots) / sizeof(tc_3000_500_slots[0]);

struct tc_builtin tc_3000_500_builtins[] = {
	{ "FLAMG-IO",	7, 0x00000000, C(TC_3000_500_DEV_IOASIC),	},
	{ "PMAGB-BA",	7, 0x02000000, C(TC_3000_500_DEV_CXTURBO),	},
	{ "PMAZ-DS ",	6, 0x00000000, C(TC_3000_500_DEV_TCDS),		},
};
int tc_3000_500_nbuiltins =
    sizeof(tc_3000_500_builtins) / sizeof(tc_3000_500_builtins[0]);

u_int32_t tc_3000_500_intrbits[TC_3000_500_NCOOKIES] = {
	TC_3000_500_IR_OPT0,
	TC_3000_500_IR_OPT1,
	TC_3000_500_IR_OPT2,
	TC_3000_500_IR_OPT3,
	TC_3000_500_IR_OPT4,
	TC_3000_500_IR_OPT5,
	TC_3000_500_IR_TCDS,
	TC_3000_500_IR_IOASIC,
	TC_3000_500_IR_CXTURBO,
};

struct tcintr {
	int	(*tci_func) __P((void *));
	void	*tci_arg;
} tc_3000_500_intr[TC_3000_500_NCOOKIES];

void
tc_3000_500_intr_setup()
{
	u_long i;
	u_int32_t imr;

	/*
	 * Disable all slot interrupts.
	 */
	imr = *(volatile u_int32_t *)TC_3000_500_IMR_READ;
	for (i = 0; i < TC_3000_500_NCOOKIES; i++)
		imr |= tc_3000_500_intrbits[i];
	*(volatile u_int32_t *)TC_3000_500_IMR_WRITE = imr;
	tc_mb();

        /*
	 * Set up interrupt handlers.
	 */
        for (i = 0; i < TC_3000_500_NCOOKIES; i++) {
		tc_3000_500_intr[i].tci_func = tc_3000_500_intrnull;
		tc_3000_500_intr[i].tci_arg = (void *)i;
        }
}

void
tc_3000_500_intr_establish(tcadev, cookie, level, func, arg)
	struct device *tcadev;
	void *cookie, *arg;
	tc_intrlevel_t level;
	int (*func) __P((void *));
{
	u_long dev = (u_long)cookie;
	u_int32_t imr;

#ifdef DIAGNOSTIC
	/* XXX bounds-check cookie. */
#endif

	if (tc_3000_500_intr[dev].tci_func != tc_3000_500_intrnull)
		panic("tc_3000_500_intr_establish: cookie %d twice", dev);

	tc_3000_500_intr[dev].tci_func = func;
	tc_3000_500_intr[dev].tci_arg = arg;

	imr = *(volatile u_int32_t *)TC_3000_500_IMR_READ;
	imr &= ~tc_3000_500_intrbits[dev];
	*(volatile u_int32_t *)TC_3000_500_IMR_WRITE = imr;
	tc_mb();
}

void
tc_3000_500_intr_disestablish(tcadev, cookie)
	struct device *tcadev;
	void *cookie;
{
	u_long dev = (u_long)cookie;
	u_int32_t imr;

#ifdef DIAGNOSTIC
	/* XXX bounds-check cookie. */
#endif

	if (tc_3000_500_intr[dev].tci_func == tc_3000_500_intrnull)
		panic("tc_3000_500_intr_disestablish: cookie %d bad intr",
		    dev);

	imr = *(volatile u_int32_t *)TC_3000_500_IMR_READ;
	imr |= tc_3000_500_intrbits[dev];
	*(volatile u_int32_t *)TC_3000_500_IMR_WRITE = imr;
	tc_mb();

	tc_3000_500_intr[dev].tci_func = tc_3000_500_intrnull;
	tc_3000_500_intr[dev].tci_arg = (void *)dev;
}

int
tc_3000_500_intrnull(val)
	void *val;
{

	panic("tc_3000_500_intrnull: uncaught TC intr for cookie %ld\n",
	    (u_long)val);
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
		tc_syncbus();
		ir = *(volatile u_int32_t *)TC_3000_500_IR_CLEAR;

		ifound = 0;
#define	CHECKINTR(slot)							\
		if (ir & tc_3000_500_intrbits[slot]) {			\
			ifound = 1;					\
			(*tc_3000_500_intr[slot].tci_func)		\
			    (tc_3000_500_intr[slot].tci_arg);		\
		}
		/* Do them in order of priority; highest slot # first. */
		CHECKINTR(TC_3000_500_DEV_CXTURBO);
		CHECKINTR(TC_3000_500_DEV_IOASIC);
		CHECKINTR(TC_3000_500_DEV_TCDS);
		CHECKINTR(TC_3000_500_DEV_OPT5);
		CHECKINTR(TC_3000_500_DEV_OPT4);
		CHECKINTR(TC_3000_500_DEV_OPT3);
		CHECKINTR(TC_3000_500_DEV_OPT2);
		CHECKINTR(TC_3000_500_DEV_OPT1);
		CHECKINTR(TC_3000_500_DEV_OPT0);
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
	tc_mb();
	splx(s);
}
