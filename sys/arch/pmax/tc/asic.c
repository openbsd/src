/*	$NetBSD: asic.c,v 1.6 1995/09/25 20:33:28 jonathan Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Keith Bostic, Chris G. Demetriou, Jonathan Stone
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

#if 0
#include <machine/rpb.h>
#include <alpha/tc/tc.h>
#include <alpha/tc/asic.h>
#endif

#ifdef pmax
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/asic.h>
#include <pmax/pmax/kn01.h>
#include <pmax/pmax/kn02.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/maxine.h>
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/turbochannel.h>	/* interrupt enable declaration */

#include <pmax/pmax/kn03.h>
#include <pmax/pmax/kmin.h>
#include <machine/machConst.h>
#include <pmax/pmax/nameglue.h>

extern int cputype;
#endif

struct asic_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
	caddr_t	sc_base;
};

/* Definition of the driver for autoconfig. */
int	asicmatch __P((struct device *, void *, void *));
void	asicattach __P((struct device *, struct device *, void *));
int     asicprint(void *, char *);
struct cfdriver ioasiccd =
    { NULL, "asic", asicmatch, asicattach, DV_DULL, sizeof(struct asic_softc) };

void    asic_intr_establish __P((struct confargs *, intr_handler_t,
				 intr_arg_t));
void    asic_intr_disestablish __P((struct confargs *));
caddr_t asic_cvtaddr __P((struct confargs *));
int     asic_matchname __P((struct confargs *, char *));

#ifndef pmax
int	asic_intr __P((void *));
#endif

int	asic_intrnull __P((intr_arg_t));

struct asic_slot {
	struct confargs	as_ca;
	u_int		as_bits;
	intr_handler_t	as_handler;
	void		*as_val;
};

#ifdef	pmax
struct asic_slot *asic_slots;

#include "ds-asic-conf.c"

#endif

#ifdef alpha
struct asic_slot asic_slots[ASIC_MAX_NSLOTS] =

{
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
#endif	/*alpha*/

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

	/* The 3MAX (kn02) is special. */
	if (BUS_MATCHNAME(ca, KN02_ASIC_NAME)) {
		printf("(configuring KN02 system slot as asic)\n");
		goto gotasic;
	}

	/* Make sure that we're looking for this type of device. */
	if (!BUS_MATCHNAME(ca, "IOCTL   "))
		return (0);
gotasic:

	/* See if the unit number is valid. */
	switch (cputype) {
	case DS_3MIN:
		if (cf->cf_unit > 0)
			return (0);
		asic_slots = kn03_asic_slots;	/* XXX - 3min same as kn03? */
		break;
	case DS_MAXINE:
		if (cf->cf_unit > 0)
			return (0);
		asic_slots = xine_asic_slots;
		break;
	case DS_3MAX:
		if (cf->cf_unit > 0)
			return (0);
		asic_slots = kn02_asic_slots;
		break;
	case DS_3MAXPLUS:
		if (cf->cf_unit > 0)
			return (0);
		asic_slots = kn03_asic_slots;
		break;
	default:
		printf("no ASIC config table for this machine\n");
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

	if (asic_slots == NULL)
		panic("asicattach: no asic_slot map\n");

	sc->sc_base = BUS_CVTADDR(ca);
	asic_base = sc->sc_base;			/* XXX XXX XXX */

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_ASIC;
	sc->sc_bus.ab_intr_establish = asic_intr_establish;
	sc->sc_bus.ab_intr_disestablish = asic_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = asic_cvtaddr;
	sc->sc_bus.ab_matchname = asic_matchname;

#ifdef pmax
	printf("\n");
#else	/* Alpha AXP: select ASIC speed  */
#ifdef DEC_3000_300
	if (cputype == ST_DEC_3000_300) {
		*(volatile u_int *)ASIC_REG_CSR(sc->sc_base) |=
		    ASIC_CSR_FASTMODE;
		MB();
		printf(": slow mode\n");
	} else
#endif /*DEC_3000_300*/
		printf(": fast mode\n");
	
	/* Decstations use hand-craft code to enable asic interrupts */
	BUS_INTR_ESTABLISH(ca, asic_intr, sc);

#endif 	/* Alpha AXP: select ASIC speed  */


/* The MAXINE has seven pseudo-slots in its system slot */
#define ASIC_MAX_NSLOTS 7 /*XXX*/

        /* Try to configure each CPU-internal device */
        for (i = 0; i < ASIC_MAX_NSLOTS; i++) {

#ifdef DEBUG_ASIC
		printf("asicattach: entry %d\n", i);		/*XXX*/
#endif

                nca = &asic_slots[i].as_ca;
		if (nca == NULL) panic ("bad asic table\n");
		if (nca->ca_name == NULL && nca->ca_bus == NULL)
			break;
                nca->ca_bus = &sc->sc_bus;

#ifdef DEBUG_ASIC
		printf(" adding %s subslot %d offset %x\n",	/*XXX*/
		       nca->ca_name, nca->ca_slot, nca->ca_offset);
#endif

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
	printf(" priority %d", ca->ca_slotpri);
	return (UNCONF);
}

/*
 * Save interrupt slotname and enable mask (??)
 * On decstaitons this isn't useful, as the turbochannel
 * decstations all have incompatible ways of mapping interrupts
 * to IO ASIC or r3000 interrupt bits.
 * Instead of writing "as_bits" directly into an IOASIC interrupt-enable
 * register,  Decstations use a  machine-dependent function that
 * interpret (pseudo-) slot numbers and do ``the right thing'' to enable
 * or disable  interrupts for a specific slot (or pseudo-slot), by
 * slot number, on that given CPU.
 */
void
asic_intr_establish(ca, handler, val)
	struct confargs *ca;
	intr_handler_t handler;
        intr_arg_t val;
{

#ifdef DIAGNOSTIC
#ifdef alpha	/*XXX*/
	if (ca->ca_slot == ASIC_SLOT_RTC)
		panic("setting clock interrupt incorrectly");
#endif /*alpha*/
#endif	/*DIAGNOSTIC*/

	/* XXX SHOULD NOT BE THIS LITERAL */
	if (asic_slots[ca->ca_slot].as_handler != asic_intrnull)
		/*panic*/ printf("asic_intr_establish: slot %d twice", ca->ca_slot);

	/*
	 * XXX  We need to invent a better interface to machine-dependent
	 * interrupt-enable code, or redo the Decstation configuration
	 * tables with unused entries, so that slot is always equal
	 * to "priority" (software pseudo-slot number).
	 */
#ifdef pmax
#ifdef	DEBUG_ASIC
	printf("asic:%s%d:  intr for entry %d(%d) slot %d\n", 
		 ca->ca_name, val, ca->ca_slot, ca->ca_slotpri,
		 asic_slots[ca->ca_slot].as_val);
#endif	/*DEBUG*/
	tc_enable_interrupt(ca->ca_slotpri, handler, val, 1);

#else	/* Alpha AXP */

	/* Save handler info so it can be enabled later (??) */
	asic_slots[ca->ca_slot].as_handler = handler;
	asic_slots[ca->ca_slot].as_val = val;
#endif	/* Alpha AXP */
}

void
asic_intr_disestablish(ca)
	struct confargs *ca;
{

#ifdef pmax
	panic("asic_intr_disestablish: shouldn't ever be called\n");
#else
	if (ca->ca_slot == ASIC_SLOT_RTC)
	        panic("asic_intr_disestablish: can't do clock interrupt");

	/* XXX SHOULD NOT BE THIS LITERAL */
	if (asic_slots[ca->ca_slot].as_handler == asic_intrnull)
		panic("asic_intr_disestablish: slot %d missing intr",
		    ca->ca_slot);

	asic_slots[ca->ca_slot].as_handler = dsasic_intrnull;
	asic_slots[ca->ca_slot].as_val = (void *)(long)ca->ca_slot;
#endif
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

#ifndef	pmax
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
		MB();
		MAGIC_READ;
		MB();

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
#endif /*!pmax*/

int
asic_intrnull(val)
	intr_arg_t val;
{

        panic("uncaught IOCTL ASIC intr for slot %ld\n", (long)val);
}
