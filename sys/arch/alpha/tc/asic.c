/*	$NetBSD: asic.c,v 1.5 1995/08/03 00:52:00 cgd Exp $	*/

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
#include <alpha/tc/asic.h>

struct asic_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
	caddr_t	sc_base;
};

/* Definition of the driver for autoconfig. */
int	asicmatch __P((struct device *, void *, void *));
void	asicattach __P((struct device *, struct device *, void *));
int     asicprint(void *, char *);
struct cfdriver asiccd =
    { NULL, "asic", asicmatch, asicattach, DV_DULL, sizeof(struct asic_softc) };

void    asic_intr_establish __P((struct confargs *, int (*)(void *), void *));
void    asic_intr_disestablish __P((struct confargs *));
caddr_t asic_cvtaddr __P((struct confargs *));
int     asic_matchname __P((struct confargs *, char *));

int	asic_intr __P((void *));
int	asic_intrnull __P((void *));

struct asic_slot {
	struct confargs	as_ca;
	u_int64_t	as_bits;
	intr_handler_t	as_handler;
	void		*as_val;
} asic_slots[ASIC_MAX_NSLOTS] = {
	{ { "lance",		/* XXX */ 0, 0x000c0000, },
	    ASIC_INTR_LANCE, asic_intrnull, (void *)(long)ASIC_SLOT_LANCE, },
	{ { "scc",		/* XXX */ 1, 0x00100000, },
	    ASIC_INTR_SCC_0, asic_intrnull, (void *)(long)ASIC_SLOT_SCC0, },
	{ { "scc",		/* XXX */ 2, 0x00180000, },
	    ASIC_INTR_SCC_1, asic_intrnull, (void *)(long)ASIC_SLOT_SCC1, },
	{ { "dallas_rtc",	/* XXX */ 3, 0x00200000, },
	    0, asic_intrnull, (void *)(long)ASIC_SLOT_RTC, },
	{ { "AMD79c30",		/* XXX */ 4, 0x00240000, },
	    0 /* XXX */, asic_intrnull, (void *)(long)ASIC_SLOT_ISDN, },
};

caddr_t asic_base;		/* XXX XXX XXX */

int
asicmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

	/* It can only occur on the turbochannel, anyway. */
	if (ca->ca_bus->ab_type != BUS_TC)
		return (0);

	/* Make sure that we're looking for this type of device. */
	if (!BUS_MATCHNAME(ca, "IOCTL   "))
		return (0);

	/* See if the unit number is valid. */
	switch (hwrpb->rpb_type) {
#if defined(DEC_3000_500) || defined(DEC_3000_300)
	case ST_DEC_3000_500:
	case ST_DEC_3000_300:
		if (cf->cf_unit > 0)
			return (0);
		break;
#endif
	default:
		return (0);
	}

	return (1);
}

void
asicattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct asic_softc *sc = (struct asic_softc *)self;
	struct confargs *ca = aux;
	struct confargs *nca;
	int i;
	extern int cputype;

	sc->sc_base = BUS_CVTADDR(ca);
	asic_base = sc->sc_base;			/* XXX XXX XXX */

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_ASIC;
	sc->sc_bus.ab_intr_establish = asic_intr_establish;
	sc->sc_bus.ab_intr_disestablish = asic_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = asic_cvtaddr;
	sc->sc_bus.ab_matchname = asic_matchname;

	BUS_INTR_ESTABLISH(ca, asic_intr, sc);

#ifdef DEC_3000_300
	if (cputype == ST_DEC_3000_300) {
		*(volatile u_int *)ASIC_REG_CSR(sc->sc_base) |=
		    ASIC_CSR_FASTMODE;
		wbflush();
		printf(": slow mode\n");
	} else
#endif
		printf(": fast mode\n");

        /* Try to configure each CPU-internal device */
        for (i = 0; i < ASIC_MAX_NSLOTS; i++) {
                nca = &asic_slots[i].as_ca;
                nca->ca_bus = &sc->sc_bus;

                /* Tell the autoconfig machinery we've found the hardware. */
                config_found(self, nca, asicprint);
        }
}

int
asicprint(aux, pnp)
	void *aux;
	char *pnp;
{
	struct confargs *ca = aux;

	if (pnp)
		printf("%s at %s", ca->ca_name, pnp);
	printf(" offset 0x%lx", ca->ca_offset);
	return (UNCONF);
}

void
asic_intr_establish(ca, handler, val)
	struct confargs *ca;
	int (*handler) __P((void *));
	void *val;
{

#ifdef DIAGNOSTIC
	if (ca->ca_slot == ASIC_SLOT_RTC)
		panic("setting clock interrupt incorrectly");
#endif

	/* XXX SHOULD NOT BE THIS LITERAL */
	if (asic_slots[ca->ca_slot].as_handler != asic_intrnull)
		panic("asic_intr_establish: slot %d twice", ca->ca_slot);

	asic_slots[ca->ca_slot].as_handler = handler;
	asic_slots[ca->ca_slot].as_val = val;
}

void
asic_intr_disestablish(ca)
	struct confargs *ca;
{

	if (ca->ca_slot == ASIC_SLOT_RTC)
	        panic("asic_intr_disestablish: can'd do clock interrupt");

	/* XXX SHOULD NOT BE THIS LITERAL */
	if (asic_slots[ca->ca_slot].as_handler == asic_intrnull)
		panic("asic_intr_disestablish: slot %d missing intr",
		    ca->ca_slot);

	asic_slots[ca->ca_slot].as_handler = asic_intrnull;
	asic_slots[ca->ca_slot].as_val = (void *)(long)ca->ca_slot;
}

caddr_t
asic_cvtaddr(ca)
	struct confargs *ca;
{

	return
	    (((struct asic_softc *)ca->ca_bus->ab_dv)->sc_base + ca->ca_offset);
}

int
asic_matchname(ca, name)
	struct confargs *ca;
	char *name;
{

	return (strcmp(name, ca->ca_name) == 0);
}

/*
 * asic_intr --
 *	ASIC interrupt handler.
 */
int
asic_intr(val)
	void *val;
{
	register struct asic_softc *sc = val;
	register int i, ifound;
	int gifound;
	u_int32_t sir, junk;
	volatile u_int32_t *sirp, *junkp;

	sirp = (volatile u_int32_t *)ASIC_REG_INTR(sc->sc_base);

	gifound = 0;
	do {
		ifound = 0;
		wbflush();
		MAGIC_READ;
		wbflush();

		sir = *sirp;
		for (i = 0; i < ASIC_MAX_NSLOTS; i++)
			if (sir & asic_slots[i].as_bits) {
				(void)(*asic_slots[i].as_handler)
				    (asic_slots[i].as_val);
				ifound = 1;
			}
		gifound |= ifound;
	} while (ifound);

	return (gifound);
}

int
asic_intrnull(val)
        void *val;
{

        panic("uncaught IOCTL ASIC intr for slot %ld\n", (long)val);
}

#ifdef DEC_3000_500
/*
 * flamingo_set_leds --
 *	Set the LEDs on the 400/500/600/800's.
 */
void
flamingo_set_leds(value)
	u_int value;
{
	register struct asic_softc *sc = asiccd.cd_devs[0];

	/*
	 * The 500's use the 7th bit of the SSR for FEPROM
	 * selection.
	 */
	*(volatile u_int *)ASIC_REG_CSR(sc->sc_base) &= ~0x7f;
	*(volatile u_int *)ASIC_REG_CSR(sc->sc_base) |= value & 0x7f;
	wbflush();
	DELAY(10000);
}
#endif
