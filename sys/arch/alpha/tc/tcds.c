/*	$NetBSD: tcds.c,v 1.5 1995/08/03 00:52:39 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
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

#include <machine/autoconf.h>
#include <machine/pte.h>
#include <machine/rpb.h>

#include <alpha/tc/tc.h>
#include <alpha/tc/tcds_dmavar.h>
#include <alpha/tc/tcds.h>

struct tcds_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
	caddr_t	sc_base;
};

/* Definition of the driver for autoconfig. */
int	tcdsmatch __P((struct device *, void *, void *));
void	tcdsattach __P((struct device *, struct device *, void *));
int     tcdsprint(void *, char *);
struct cfdriver tcdscd =
    { NULL, "tcds", tcdsmatch, tcdsattach, DV_DULL, sizeof(struct tcds_softc) };

void    tcds_intr_establish __P((struct confargs *, int (*)(void *), void *));
void    tcds_intr_disestablish __P((struct confargs *));
caddr_t tcds_cvtaddr __P((struct confargs *));
int     tcds_matchname __P((struct confargs *, char *));

int	tcds_intr __P((void *));
int	tcds_intrnull __P((void *));

#define	TCDS_SLOT_SCSI0	0
#define	TCDS_SLOT_SCSI1	1

struct tcds_slot {
	struct confargs	ts_ca;
	intr_handler_t	ts_handler;
	void		*ts_val;
} tcds_slots[] = {
	{ { "esp",	0, 0x0, },
	    tcds_intrnull, (void *)(long)TCDS_SLOT_SCSI0, },
	{ { "esp",	1, 0x0, },
	    tcds_intrnull, (void *)(long)TCDS_SLOT_SCSI1, },
};

int
tcdsmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;

	/* It can only occur on the turbochannel, anyway. */
	if (ca->ca_bus->ab_type != BUS_TC)
		return (0);

	/* Make sure that we're looking for this type of device. */
	if (!BUS_MATCHNAME(ca, "PMAZ-DS "))
		return (0);

	return (1);
}

void
tcdsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct tcds_softc *sc = (struct tcds_softc *)self;
	struct confargs *ca = aux;
	struct confargs *nca;
	volatile u_int32_t *cir, *imer;
	int i;
	extern int cputype;

	printf("\n");

	sc->sc_base = BUS_CVTADDR(ca);

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_TCDS;
	sc->sc_bus.ab_intr_establish = tcds_intr_establish;
	sc->sc_bus.ab_intr_disestablish = tcds_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = tcds_cvtaddr;
	sc->sc_bus.ab_matchname = tcds_matchname;

	BUS_INTR_ESTABLISH(ca, tcds_intr, sc);

	/*
	 * XXX
	 * IMER apparently has some random (or, not so random, but still
	 * not useful) bits set in it when the system boots.  Clear it.
	 */
	imer = TCDS_REG(sc->sc_base, TCDS_IMER);
	*imer = 0;
	wbflush();

	/* find the hardware attached to the TCDS ASIC */
	nca = &tcds_slots[TCDS_SLOT_SCSI0].ts_ca;
	nca->ca_bus = &sc->sc_bus;
	config_found(self, nca, tcdsprint);

	/* the second SCSI chip isn't present on the 3000/300 series. */
	if (cputype != ST_DEC_3000_300) {
		nca = &tcds_slots[TCDS_SLOT_SCSI1].ts_ca;
		nca->ca_bus = &sc->sc_bus;
		config_found(self, nca, tcdsprint);
	}
}

int
tcdsprint(aux, pnp)
	void *aux;
	char *pnp;
{
	struct confargs *ca = aux;

	if (pnp)
		printf("%s at %s", ca->ca_name, pnp);
	printf(" slot 0x%lx", ca->ca_slot);
	return (UNCONF);
}

void
tcds_intr_establish(ca, handler, val)
	struct confargs *ca;
	int (*handler) __P((void *));
	void *val;
{
	if (tcds_slots[ca->ca_slot].ts_handler != tcds_intrnull)
		panic("tcds_intr_establish: slot %d twice", ca->ca_slot);

	tcds_slots[ca->ca_slot].ts_handler = handler;
	tcds_slots[ca->ca_slot].ts_val = val;

	switch (ca->ca_slot) {
	case TCDS_SLOT_SCSI0:
		tcds_scsi_reset(0);
		break;

	case TCDS_SLOT_SCSI1:
		tcds_scsi_reset(1);
		break;

	default:
		panic("tcds_intr_establish: unknown slot number %d",
		    ca->ca_slot);
		/* NOTREACHED */
	}
}

void
tcds_intr_disestablish(ca)
	struct confargs *ca;
{
	if (tcds_slots[ca->ca_slot].ts_handler == tcds_intrnull)
		panic("tcds_intr_disestablish: slot %d missing intr",
		    ca->ca_slot);

	tcds_slots[ca->ca_slot].ts_handler = tcds_intrnull;
	tcds_slots[ca->ca_slot].ts_val = (void *)(long)ca->ca_slot;

	switch (ca->ca_slot) {
	case TCDS_SLOT_SCSI0:
		tcds_dma_disable(0);
		tcds_scsi_disable(0);
		break;

	case TCDS_SLOT_SCSI1:
		tcds_dma_disable(1);
		tcds_scsi_disable(1);
		break;

	default:
		panic("tcds_intr_disestablish: unknown slot number %d",
		    ca->ca_slot);
		/* NOTREACHED */
	}
}

caddr_t
tcds_cvtaddr(ca)
	struct confargs *ca;
{

	return
	    (((struct tcds_softc *)ca->ca_bus->ab_dv)->sc_base + ca->ca_offset);
}

int
tcds_matchname(ca, name)
	struct confargs *ca;
	char *name;
{

	return (strcmp(name, ca->ca_name) == 0);
}

int
tcds_intrnull(val)
	void *val;
{

	panic("uncaught TCDS ASIC intr for slot %ld\n", (long)val);
}

void
tcds_scsi_reset(unit)
	int unit;
{
	struct tcds_softc *sc = tcdscd.cd_devs[0];
	volatile u_int32_t *cir;

	tcds_dma_disable(unit);
	tcds_scsi_disable(unit);

	/* XXX: Clear/set IOSLOT/PBS bits. */
	cir = TCDS_REG(sc->sc_base, TCDS_CIR);
	switch (unit) {
	case 0:
		TCDS_CIR_CLR(*cir, TCDS_CIR_SCSI0_RESET);
		wbflush();
		DELAY(1);			/* XXX */
		TCDS_CIR_SET(*cir, TCDS_CIR_SCSI0_RESET);
		wbflush();
		break;

	case 1:
		TCDS_CIR_CLR(*cir, TCDS_CIR_SCSI1_RESET);
		wbflush();
		DELAY(1);			/* XXX */
		TCDS_CIR_SET(*cir, TCDS_CIR_SCSI1_RESET);
		wbflush();
		break;

	default:
		panic("tcds_scsi_disable: unknown unit number\n", unit);
		/* NOTREACHED */
	}

	tcds_scsi_enable(unit);
	tcds_dma_enable(unit);
}

void
tcds_scsi_enable(unit)
	int unit;
{
	struct tcds_softc *sc = tcdscd.cd_devs[0];
	volatile u_int32_t *imer;

	imer = TCDS_REG(sc->sc_base, TCDS_IMER);

	/*
	 * XXX
	 * Should we be setting all the "interrupt bits" in the IMER?
	 * Do we need to set a bit in the mask so that SCSI DMA errors
	 * cause interrupts?
	 */
	switch (unit) {
	case 0:
		*imer |= TCDS_IMER_SCSI0_MASK | TCDS_IMER_SCSI0_ENB;
		wbflush();
		break;

	case 1:
		*imer |= TCDS_IMER_SCSI1_MASK | TCDS_IMER_SCSI1_ENB;
		wbflush();
		break;

	default:
		panic("tcds_scsi_enable: unknown unit number\n", unit);
		/* NOTREACHED */
	}
}

void
tcds_scsi_disable(unit)
	int unit;
{
	struct tcds_softc *sc = tcdscd.cd_devs[0];
	volatile u_int32_t *imer;

	imer = TCDS_REG(sc->sc_base, TCDS_IMER);

	switch (unit) {
	case 0:
		*imer &= ~(TCDS_IMER_SCSI0_MASK | TCDS_IMER_SCSI0_ENB);
		wbflush();
		break;

	case 1:
		*imer &= ~(TCDS_IMER_SCSI1_MASK | TCDS_IMER_SCSI1_ENB);
		wbflush();
		break;

	default:
		panic("tcds_scsi_disable: unknown unit number\n", unit);
		/* NOTREACHED */
	}
}

void
tcds_dma_init(dsc, unit)
	struct dma_softc *dsc;
	int unit;
{
	struct tcds_softc *sc = tcdscd.cd_devs[0];

	switch (unit) {
	case 0:
		dsc->sda = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_ADDR);
		dsc->dic = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_INTR);
		dsc->dud0 = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_DUD0);
		dsc->dud1 = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_DUD1);
		break;

	case 1:
		dsc->sda = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_ADDR);
		dsc->dic = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_INTR);
		dsc->dud0 = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_DUD0);
		dsc->dud1 = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_DUD1);
		break;

	default:
		panic("tcds_dma_init: unknown unit number\n", unit);
		/* NOTREACHED */
	}
}

void
tcds_dma_enable(unit)
	int unit;
{
	struct tcds_softc *sc = tcdscd.cd_devs[0];
	volatile u_int32_t *cir;

	cir = TCDS_REG(sc->sc_base, TCDS_CIR);

	/* XXX Clear/set IOSLOT/PBS bits. */
	switch (unit) {
	case 0:
		TCDS_CIR_SET(*cir, TCDS_CIR_SCSI0_DMAENA);
		wbflush();
		break;

	case 1:
		TCDS_CIR_SET(*cir, TCDS_CIR_SCSI1_DMAENA);
		wbflush();
		break;

	default:
		panic("tcds_dma_enable: unknown unit number\n", unit);
		/* NOTREACHED */
	}
}

void
tcds_dma_disable(unit)
	int unit;
{
	struct tcds_softc *sc = tcdscd.cd_devs[0];
	volatile u_int32_t *cir;

	cir = TCDS_REG(sc->sc_base, TCDS_CIR);

	/* XXX Clear/set IOSLOT/PBS bits. */
	switch (unit) {
	case 0:
		TCDS_CIR_CLR(*cir, TCDS_CIR_SCSI0_DMAENA);
		wbflush();
		break;

	case 1:
		TCDS_CIR_CLR(*cir, TCDS_CIR_SCSI1_DMAENA);
		wbflush();
		break;

	default:
		panic("tcds_dma_disable: unknown unit number\n", unit);
		/* NOTREACHED */
	}
}

int
tcds_scsi_isintr(unit, clear)
	int unit, clear;
{
	struct tcds_softc *sc = tcdscd.cd_devs[0];
	volatile u_int32_t *cir, ir;

	cir = TCDS_REG(sc->sc_base, TCDS_CIR);
	ir = *cir;
	wbflush();

	switch (unit) {
	case 0:
		if (ir & TCDS_CIR_SCSI0_INT) {
			if (clear) {
				TCDS_CIR_CLR(*cir, TCDS_CIR_SCSI0_INT);
				wbflush();
			}
			return (1);
		}
		break;

	case 1:
		if (ir & TCDS_CIR_SCSI1_INT) {
			if (clear) {
				TCDS_CIR_CLR(*cir, TCDS_CIR_SCSI1_INT);
				wbflush();
			}
			return (1);
		}
		break;

	default:
		panic("tcds_scsi_isintr: unknown unit number\n", unit);
		/* NOTREACHED */
	}
	return (0);
}

int
tcds_scsi_iserr(dsc)
	struct dma_softc *dsc;
{
	struct tcds_softc *sc = tcdscd.cd_devs[0];
	volatile u_int32_t *cir, ir;

	cir = TCDS_REG(sc->sc_base, TCDS_CIR);
	ir = *cir;
	wbflush();

	if (ir & SCSI_CIR_ERROR) {
		printf("%s: error <CIR = %x>\n", dsc->sc_dev.dv_xname, ir);
		return (1);
	}
	return (0);
}

/*
 * tcds_intr --
 *	TCDS ASIC interrupt handler.
 */
int
tcds_intr(val)
	void *val;
{
	struct tcds_softc *sc;
	volatile u_int32_t *cir;
	u_int32_t ir;

	sc = val;

	/*
	 * XXX
	 * Copy and clear (gag!) the interrupts.
	 */
	cir = TCDS_REG(sc->sc_base, TCDS_CIR);
	ir = *cir;
	wbflush();
	TCDS_CIR_CLR(*cir, TCDS_CIR_ALLINTR);
	wbflush();
	MAGIC_READ;
	wbflush();

#define	CHECKINTR(slot, bits)						\
	if (ir & bits) {						\
		(void)(*tcds_slots[slot].ts_handler)			\
		    (tcds_slots[slot].ts_val);				\
	}
	CHECKINTR(TCDS_SLOT_SCSI0, TCDS_CIR_SCSI0_INT);
	CHECKINTR(TCDS_SLOT_SCSI1, TCDS_CIR_SCSI1_INT);
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
}
