/*	$NetBSD: asic.c,v 1.16 1997/05/24 09:30:27 jonathan Exp $	*/

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
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>

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
	tc_addr_t sc_base;
};

/* Definition of the driver for autoconfig. */
int	asicmatch __P((struct device *, void *, void *));
void	asicattach __P((struct device *, struct device *, void *));
int     asicprint(void *, const char *);

/* Device locators. */
#define	ioasiccf_offset	cf_loc[0]		/* offset */

#define	IOASIC_OFFSET_UNKNOWN	-1

struct cfattach ioasic_ca = {
	sizeof(struct asic_softc), asicmatch, asicattach
};

struct cfdriver ioasic_cd = {
	NULL, "asic", DV_DULL
};

void    asic_intr_establish __P((struct confargs *, intr_handler_t,
				 intr_arg_t));
void    asic_intr_disestablish __P((struct confargs *));
caddr_t ioasic_cvtaddr __P((struct confargs *));

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
/*#define IOASIC_DEBUG*/

struct asic_slot *asic_slots;
#include "ds-asic-conf.c"
#endif	/*pmax*/


#ifdef IOASIC_DEBUG
#define IOASIC_DPRINTF(x)	printf x
#else
#define IOASIC_DPRINTF(x)	do { if (0) printf x ; } while (0)
#endif

int
asicmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct tc_attach_args *ta = aux;

	IOASIC_DPRINTF(("asicmatch: %s slot %d offset 0x%x pri %d\n",
		ta->ta_modname, ta->ta_slot, ta->ta_offset, (int)ta->ta_cookie));

	/* An IOCTL asic can only occur on the turbochannel, anyway. */
#ifdef notyet
	if (parent != &tccd)
		return (0);
#endif

	/* The 3MAX (kn02) is special. */
	if (TC_BUS_MATCHNAME(ta, KN02_ASIC_NAME)) {
		printf("(configuring KN02 system slot as asic)\n");
		goto gotasic;
	}

	/* Make sure that we're looking for this type of device. */
	if (!TC_BUS_MATCHNAME(ta, "IOCTL   "))
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
	struct tc_attach_args *ta = aux;
	struct confargs *nca;
	struct ioasicdev_attach_args ioasicdev;
	int i;
	extern int cputype;

	if (asic_slots == NULL)
		panic("asicattach: no asic_slot map");

	IOASIC_DPRINTF(("asicattach: %s\n", sc->sc_dv.dv_xname));

	sc->sc_base = ta->ta_addr;

	ioasic_base = sc->sc_base;			/* XXX XXX XXX */

#ifdef pmax
	printf("\n");
#else	/* Alpha AXP: select ASIC speed  */
#ifdef DEC_3000_300
	if (cputype == ST_DEC_3000_300) {
		*(volatile u_int *)IOASIC_REG_CSR(sc->sc_base) |=
		    IOASIC_CSR_FASTMODE;
		MB();
		printf(": slow mode\n");
	} else
#endif /*DEC_3000_300*/
		printf(": fast mode\n");
	
	/* Decstations use hand-craft code to enable asic interrupts */
	BUS_INTR_ESTABLISH(ta, asic_intr, sc);

#endif 	/* Alpha AXP: select ASIC speed  */


/* The MAXINE has seven pseudo-slots in its system slot */
#define ASIC_MAX_NSLOTS 7 /*XXX*/

        /* Try to configure each CPU-internal device */
        for (i = 0; i < ASIC_MAX_NSLOTS; i++) {

		IOASIC_DPRINTF(("asicattach: entry %d, base addr %x\n",
		       i, sc->sc_base));

                nca = &asic_slots[i].as_ca;
		if (nca == NULL) panic ("bad asic table");
		if (nca->ca_name == NULL || nca->ca_name[0] == 0)
			break;
		nca->ca_addr = ((u_int)sc->sc_base) + nca->ca_offset;

		IOASIC_DPRINTF((" adding %s subslot %d offset %x addr %x\n",
		       nca->ca_name, nca->ca_slot, nca->ca_offset,
		       nca->ca_addr));

		strncpy(ioasicdev.iada_modname, nca->ca_name, TC_ROM_LLEN);
		ioasicdev.iada_modname[TC_ROM_LLEN] = '\0';
		ioasicdev.iada_offset = nca->ca_offset;
		ioasicdev.iada_addr = nca->ca_addr;
		ioasicdev.iada_cookie = (void *)nca->ca_slotpri;
                /* Tell the autoconfig machinery we've found the hardware. */
                config_found(self, &ioasicdev, asicprint);
        }
	IOASIC_DPRINTF(("asicattach: done\n"));
}

int
asicprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct ioasicdev_attach_args *d = aux;

	if (pnp)
		printf("%s at %s", d->iada_modname, pnp);
	printf(" offset 0x%x", d->iada_offset);
	printf(" priority %d", (int)d->iada_cookie);
	return (UNCONF);
}

int
ioasic_submatch(match, d)
	struct cfdata *match;
	struct ioasicdev_attach_args *d;
{

	return ((match->ioasiccf_offset == d->iada_offset) ||
		(match->ioasiccf_offset == IOASIC_OFFSET_UNKNOWN));
}

/*
 * Save interrupt slotname and enable mask (??)
 * On decstations this isn't useful, as the turbochannel
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

#if defined(DIAGNOSTIC) && defined(alpha)
	if (ca->ca_slot == IOASIC_SLOT_RTC)
		panic("setting clock interrupt incorrectly");
#endif	/*defined(DIAGNOSTIC) && defined(alpha)*/

	/* XXX SHOULD NOT BE THIS LITERAL */
	if (asic_slots[ca->ca_slot].as_handler != asic_intrnull)
		/*panic*/ printf("asic_intr_establish: slot %d twice", ca->ca_slot);

	/*
	 * XXX  We need to invent a better interface to machine-dependent
	 * interrupt-enable code, or redo the Decstation configuration
	 * tables with unused entries, so that slot is always equal
	 * to "priority" (software pseudo-slot number).  FIXME.
	 */
#if defined(IOASIC_DEBUG) && 0
	printf("asic: %s:  intr for entry %d slot %d pri %d\n", 
		 ca->ca_name, ca->ca_slot, ca->ca_slotpri,
		 (int)asic_slots[ca->ca_slot].as_val);
#endif	/*IOASIC_DEBUG*/

#ifdef pmax
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
	panic("asic_intr_disestablish: shouldn't ever be called");
#else
	if (ca->ca_slot == IOASIC_SLOT_RTC)
	        panic("asic_intr_disestablish: can't do clock interrupt");

	/* XXX SHOULD NOT BE THIS LITERAL */
	if (asic_slots[ca->ca_slot].as_handler == asic_intrnull)
		panic("asic_intr_disestablish: slot %d missing intr",
		    ca->ca_slot);

	asic_slots[ca->ca_slot].as_handler = dsasic_intrnull;
	asic_slots[ca->ca_slot].as_val = (void *)(long)ca->ca_slot;
#endif
}


void
ioasic_intr_establish(dev, cookie, level, handler, val)
    struct device *dev;
    void *cookie;
    tc_intrlevel_t level;
    intr_handler_t handler;
    void *val;
{

	(*tc_enable_interrupt)((int)cookie, handler, val, 1);
}

#ifdef	alpha
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

	sirp = (volatile u_int32_t *)IOASIC_REG_INTR(sc->sc_base);

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

        panic("uncaught IOCTL ASIC intr for slot %ld", (long)val);
}


/* XXX */
char *
ioasic_lance_ether_address()
{

	return (u_char *)IOASIC_SYS_ETHER_ADDRESS(ioasic_base);
}

void
ioasic_lance_dma_setup(v)
	void *v;
{
	volatile u_int32_t *ldp;
	tc_addr_t tca;

	tca = (tc_addr_t)v;

	ldp = (volatile u_int *)IOASIC_REG_LANCE_DMAPTR(ioasic_base);
	*ldp = ((tca << 3) & ~(tc_addr_t)0x1f) | ((tca >> 29) & 0x1f);
	tc_wmb();

	*(volatile u_int32_t *)IOASIC_REG_CSR(ioasic_base) |=
	    IOASIC_CSR_DMAEN_LANCE;
	tc_mb();
}
