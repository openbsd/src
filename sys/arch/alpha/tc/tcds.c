/*	$OpenBSD: tcds.c,v 1.8 1999/01/11 05:11:05 millert Exp $	*/
/*	$NetBSD: tcds.c,v 1.16 1996/12/05 01:39:45 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Keith Bostic, Chris G. Demetriou
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/pte.h>
#include <machine/rpb.h>
#ifndef EVCNT_COUNTERS
#include <machine/intrcnt.h>
#endif

#include <dev/tc/tcreg.h>
#include <dev/tc/tcvar.h>
#include <alpha/tc/tcdsreg.h>
#include <alpha/tc/tcdsvar.h>

struct tcds_softc {
	struct	device sc_dv;
	tc_addr_t sc_base;
	void	*sc_cookie;

	volatile u_int32_t *sc_cir;
	volatile u_int32_t *sc_imer;

	struct tcds_slotconfig sc_slots[2];
};

/* Definition of the driver for autoconfig. */
#ifdef __BROKEN_INDIRECT_CONFIG
int	tcdsmatch __P((struct device *, void *, void *));
#else
int	tcdsmatch __P((struct device *, struct cfdata *, void *));
#endif
void	tcdsattach __P((struct device *, struct device *, void *));
int     tcdsprint __P((void *, const char *));

struct cfattach tcds_ca = {
	sizeof(struct tcds_softc), tcdsmatch, tcdsattach,
};

struct cfdriver tcds_cd = {
	NULL, "tcds", DV_DULL,
};

/*static*/ int	tcds_intr __P((void *));
/*static*/ int	tcds_intrnull __P((void *));

int
tcdsmatch(parent, cfdata, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *cfdata;
#else
	struct cfdata *cfdata;
#endif
	void *aux;
{
	struct tc_attach_args *ta = aux;
	extern int cputype;

	/* Make sure that we're looking for this type of device. */
	if (strncmp("PMAZ-DS ", ta->ta_modname, TC_ROM_LLEN))
		return (0);
	/* PMAZ-FS? */

	/* Check that it can actually exist. */
	if ((cputype != ST_DEC_3000_500) && (cputype != ST_DEC_3000_300))
		panic("tcdsmatch: how did we get here?");

	return (1);
}

void
tcdsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct tcds_softc *sc = (struct tcds_softc *)self;
	struct tc_attach_args *ta = aux;
	struct tcdsdev_attach_args tcdsdev;
	struct tcds_slotconfig *slotc;
	int i;
	extern int cputype;

	printf("\n");

	sc->sc_base = ta->ta_addr;
	sc->sc_cookie = ta->ta_cookie;

	sc->sc_cir = TCDS_REG(sc->sc_base, TCDS_CIR);
	sc->sc_imer = TCDS_REG(sc->sc_base, TCDS_IMER);

	tc_intr_establish(parent, sc->sc_cookie, TC_IPL_BIO, tcds_intr, sc);

	/*
	 * XXX
	 * IMER apparently has some random (or, not so random, but still
	 * not useful) bits set in it when the system boots.  Clear it.
	 */
	*sc->sc_imer = 0;
	alpha_mb();

	/* XXX Initial contents of CIR? */

	/*
	 * Set up the per-slot defintions for later use.
	 */

	/* fill in common information first */
	for (i = 0; i < 2; i++) {
		slotc = &sc->sc_slots[i];

		bzero(slotc, sizeof *slotc);	/* clear everything */

		slotc->sc_slot = i;
		slotc->sc_tcds = sc;
		slotc->sc_esp = NULL;
		slotc->sc_intrhand = tcds_intrnull;
		slotc->sc_intrarg = (void *)(long)i;
	}

	/* information for slot 0 */
	slotc = &sc->sc_slots[0];
	slotc->sc_resetbits = TCDS_CIR_SCSI0_RESET;
	slotc->sc_intrmaskbits =
	    TCDS_IMER_SCSI0_MASK | TCDS_IMER_SCSI0_ENB;
	slotc->sc_intrbits = TCDS_CIR_SCSI0_INT;
	slotc->sc_dmabits = TCDS_CIR_SCSI0_DMAENA;
	slotc->sc_errorbits = 0;				/* XXX */
	slotc->sc_sda = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_ADDR);
	slotc->sc_dic = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_INTR);
	slotc->sc_dud0 = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_DUD0);
	slotc->sc_dud1 = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_DUD1);

	/* information for slot 1 */
	slotc = &sc->sc_slots[1];
	slotc->sc_resetbits = TCDS_CIR_SCSI1_RESET;
	slotc->sc_intrmaskbits =
	    TCDS_IMER_SCSI1_MASK | TCDS_IMER_SCSI1_ENB;
	slotc->sc_intrbits = TCDS_CIR_SCSI1_INT;
	slotc->sc_dmabits = TCDS_CIR_SCSI1_DMAENA;
	slotc->sc_errorbits = 0;				/* XXX */
	slotc->sc_sda = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_ADDR);
	slotc->sc_dic = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_INTR);
	slotc->sc_dud0 = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_DUD0);
	slotc->sc_dud1 = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_DUD1);

	/* find the hardware attached to the TCDS ASIC */
	strncpy(tcdsdev.tcdsda_modname, "PMAZ-AA ", TC_ROM_LLEN);
	tcdsdev.tcdsda_slot = 0;
	tcdsdev.tcdsda_offset = 0;
	tcdsdev.tcdsda_addr = (tc_addr_t)
	    TC_DENSE_TO_SPARSE(sc->sc_base + TCDS_SCSI0_OFFSET);
	tcdsdev.tcdsda_cookie = (void *)(long)0;
	tcdsdev.tcdsda_sc = &sc->sc_slots[0];
	tcdsdev.tcdsda_id = 7;				/* XXX */
	tcdsdev.tcdsda_freq = 25000000;			/* XXX */

	tcds_scsi_reset(tcdsdev.tcdsda_sc);

	config_found(self, &tcdsdev, tcdsprint);

	/* the second SCSI chip isn't present on the 3000/300 series. */
	if (cputype != ST_DEC_3000_300) {
		strncpy(tcdsdev.tcdsda_modname, "PMAZ-AA ",
		    TC_ROM_LLEN);
		tcdsdev.tcdsda_slot = 1;
		tcdsdev.tcdsda_offset = 0;
		tcdsdev.tcdsda_addr = (tc_addr_t)
		    TC_DENSE_TO_SPARSE(sc->sc_base + TCDS_SCSI1_OFFSET);
		tcdsdev.tcdsda_cookie = (void *)(long)1;
		tcdsdev.tcdsda_sc = &sc->sc_slots[1];
		tcdsdev.tcdsda_id = 7;			/* XXX */
		tcdsdev.tcdsda_freq = 25000000;		/* XXX */

		tcds_scsi_reset(tcdsdev.tcdsda_sc);

		config_found(self, &tcdsdev, tcdsprint);
	}
}

int
tcdsprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct tc_attach_args *ta = aux;

	if (pnp)
		printf("%s at %s", ta->ta_modname, pnp);
	printf(" slot %d", ta->ta_slot);
	return (UNCONF);
}

void
tcds_intr_establish(tcds, cookie, level, func, arg)
	struct device *tcds;
	void *cookie, *arg;
	tc_intrlevel_t level;
	int (*func) __P((void *));
{
	struct tcds_softc *sc = (struct tcds_softc *)tcds;
	u_long slot;

	slot = (u_long)cookie;
#ifdef DIAGNOSTIC
	/* XXX check cookie. */
#endif

	if (sc->sc_slots[slot].sc_intrhand != tcds_intrnull)
		panic("tcds_intr_establish: cookie %d twice", slot);

	sc->sc_slots[slot].sc_intrhand = func;
	sc->sc_slots[slot].sc_intrarg = arg;
	tcds_scsi_reset(&sc->sc_slots[slot]);
}

void
tcds_intr_disestablish(tcds, cookie)
	struct device *tcds;
	void *cookie;
{
	struct tcds_softc *sc = (struct tcds_softc *)tcds;
	u_long slot;

	slot = (u_long)cookie;
#ifdef DIAGNOSTIC
	/* XXX check cookie. */
#endif

	if (sc->sc_slots[slot].sc_intrhand == tcds_intrnull)
		panic("tcds_intr_disestablish: cookie %d missing intr",
		    slot);

	sc->sc_slots[slot].sc_intrhand = tcds_intrnull;
	sc->sc_slots[slot].sc_intrarg = (void *)slot;

	tcds_dma_enable(&sc->sc_slots[slot], 0);
	tcds_scsi_enable(&sc->sc_slots[slot], 0);
}

int
tcds_intrnull(val)
	void *val;
{

	panic("tcds_intrnull: uncaught TCDS intr for cookie %ld",
	    (u_long)val);
}

void
tcds_scsi_reset(sc)
	struct tcds_slotconfig *sc;
{

	tcds_dma_enable(sc, 0);
	tcds_scsi_enable(sc, 0);

	TCDS_CIR_CLR(*sc->sc_tcds->sc_cir, sc->sc_resetbits);
	alpha_mb();
	DELAY(1);
	TCDS_CIR_SET(*sc->sc_tcds->sc_cir, sc->sc_resetbits);
	alpha_mb();

	tcds_scsi_enable(sc, 1);
	tcds_dma_enable(sc, 1);
}

void
tcds_scsi_enable(sc, on)
	struct tcds_slotconfig *sc;
	int on;
{

	if (on) 
		*sc->sc_tcds->sc_imer |= sc->sc_intrmaskbits;
	else
		*sc->sc_tcds->sc_imer &= ~sc->sc_intrmaskbits;
	alpha_mb();
}

void
tcds_dma_enable(sc, on)
	struct tcds_slotconfig *sc;
	int on;
{

	/* XXX Clear/set IOSLOT/PBS bits. */
	if (on) 
		TCDS_CIR_SET(*sc->sc_tcds->sc_cir, sc->sc_dmabits);
	else
		TCDS_CIR_CLR(*sc->sc_tcds->sc_cir, sc->sc_dmabits);
	alpha_mb();
}

int
tcds_scsi_isintr(sc, clear)
	struct tcds_slotconfig *sc;
	int clear;
{

	if ((*sc->sc_tcds->sc_cir & sc->sc_intrbits) != 0) {
		if (clear) {
			TCDS_CIR_CLR(*sc->sc_tcds->sc_cir, sc->sc_intrbits);
			alpha_mb();
		}
		return (1);
	} else
		return (0);
}

int
tcds_scsi_iserr(sc)
	struct tcds_slotconfig *sc;
{

	return ((*sc->sc_tcds->sc_cir & sc->sc_errorbits) != 0);
}

int
tcds_intr(val)
	void *val;
{
	struct tcds_softc *sc;
	u_int32_t ir;

	sc = val;

	/*
	 * XXX
	 * Copy and clear (gag!) the interrupts.
	 */
	ir = *sc->sc_cir;
	alpha_mb();
	TCDS_CIR_CLR(*sc->sc_cir, TCDS_CIR_ALLINTR);
	alpha_mb();
	tc_syncbus();
	alpha_mb();

#ifdef EVCNT_COUNTERS
	/* No interrupt counting via evcnt counters */ 
	XXX BREAK HERE XXX
#else
#define	INCRINTRCNT(slot)	intrcnt[INTRCNT_TCDS + slot]++
#endif

#define	CHECKINTR(slot)							\
	if (ir & sc->sc_slots[slot].sc_intrbits) {			\
		INCRINTRCNT(slot);					\
		(void)(*sc->sc_slots[slot].sc_intrhand)			\
		    (sc->sc_slots[slot].sc_intrarg);			\
	}
	CHECKINTR(0);
	CHECKINTR(1);
#undef CHECKINTR

#ifdef DIAGNOSTIC
	/* 
	 * Interrupts not currently handled, but would like to know if they
	 * occur.
	 *
	 * XXX
	 * Don't know if we have to set the interrupt mask and enable bits
	 * in the IMER to allow some of them to happen?
	 */
#define	PRINTINTR(msg, bits)						\
	if (ir & bits)							\
		printf(msg);
	PRINTINTR("SCSI0 DREQ interrupt.\n", TCDS_CIR_SCSI0_DREQ);
	PRINTINTR("SCSI1 DREQ interrupt.\n", TCDS_CIR_SCSI1_DREQ);
	PRINTINTR("SCSI0 prefetch interrupt.\n", TCDS_CIR_SCSI0_PREFETCH);
	PRINTINTR("SCSI1 prefetch interrupt.\n", TCDS_CIR_SCSI1_PREFETCH);
	PRINTINTR("SCSI0 DMA error.\n", TCDS_CIR_SCSI0_DMA);
	PRINTINTR("SCSI1 DMA error.\n", TCDS_CIR_SCSI1_DMA);
	PRINTINTR("SCSI0 DB parity error.\n", TCDS_CIR_SCSI0_DB);
	PRINTINTR("SCSI1 DB parity error.\n", TCDS_CIR_SCSI1_DB);
	PRINTINTR("SCSI0 DMA buffer parity error.\n", TCDS_CIR_SCSI0_DMAB_PAR);
	PRINTINTR("SCSI1 DMA buffer parity error.\n", TCDS_CIR_SCSI1_DMAB_PAR);
	PRINTINTR("SCSI0 DMA read parity error.\n", TCDS_CIR_SCSI0_DMAR_PAR);
	PRINTINTR("SCSI1 DMA read parity error.\n", TCDS_CIR_SCSI1_DMAR_PAR);
	PRINTINTR("TC write parity error.\n", TCDS_CIR_TCIOW_PAR);
	PRINTINTR("TC I/O address parity error.\n", TCDS_CIR_TCIOA_PAR);
#undef PRINTINTR
#endif

	/*
	 * XXX
	 * The MACH source had this, with the comment:
	 *	This is wrong, but machine keeps dying.
	 */
	DELAY(1);

	return (1);
}
