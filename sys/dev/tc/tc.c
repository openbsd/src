/*	$NetBSD: tc.c,v 1.1 1995/12/20 00:48:32 cgd Exp $	*/

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

#include <dev/tc/tcreg.h>
#include <dev/tc/tcvar.h>

struct tc_softc {
	struct	device sc_dv;

	int	sc_nslots;
	struct tc_slotdesc *sc_slots;

	void	(*sc_intr_establish) __P((struct device *, void *,
		    tc_intrlevel_t, int (*)(void *), void *));
	void	(*sc_intr_disestablish) __P((struct device *, void *));
};

/* Definition of the driver for autoconfig. */
int	tcmatch __P((struct device *, void *, void *));
void	tcattach __P((struct device *, struct device *, void *));
struct cfdriver tccd =
    { NULL, "tc", tcmatch, tcattach, DV_DULL, sizeof (struct tc_softc) };

int	tcprint __P((void *, char *));
int	tc_checkslot __P((tc_addr_t, char *));

int
tcmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{

	return (1);
}

void
tcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct tc_softc *sc = (struct tc_softc *)self;
	struct tc_attach_args *tc = aux;
	struct tcdev_attach_args tcdev;
	const struct tc_builtin *builtin;
	struct tc_slotdesc *slot;
	tc_addr_t tcaddr;
	int i;

	printf("\n");

	/*
	 * Save important CPU/chipset information.
	 */
	sc->sc_nslots = tc->tca_nslots;
	sc->sc_slots = tc->tca_slots;
	sc->sc_intr_establish = tc->tca_intr_establish;
	sc->sc_intr_disestablish = tc->tca_intr_disestablish;

	/*
	 * Try to configure each built-in device
	 */
	for (i = 0; i < tc->tca_nbuiltins; i++) {
		builtin = &tc->tca_builtins[i];

		/* sanity check! */
		if (builtin->tcb_slot > sc->sc_nslots)
			panic("tcattach: builtin %d slot > nslots", i);

		/*
		 * Make sure device is really there, because some
		 * built-in devices are really optional.
		 */
		tcaddr = sc->sc_slots[builtin->tcb_slot].tcs_addr +
		    builtin->tcb_offset;
		if (tc_badaddr(tcaddr))
			continue;

		/*
		 * Set up the device attachment information.
		 */
		strncpy(tcdev.tcda_modname, builtin->tcb_modname, TC_ROM_LLEN);
		tcdev.tcda_modname[TC_ROM_LLEN] = '\0';
		tcdev.tcda_slot = builtin->tcb_slot;
		tcdev.tcda_offset = builtin->tcb_offset;
		tcdev.tcda_addr = tcaddr;
		tcdev.tcda_cookie = builtin->tcb_cookie;
	
		/*
		 * Mark the slot as used, so we don't check it later.
		 */
		sc->sc_slots[builtin->tcb_slot].tcs_used = 1;

		/*
		 * Attach the device.
		 */
		config_found(self, &tcdev, tcprint);
	}

	/*
	 * Try to configure each unused slot, last to first.
	 */
	for (i = sc->sc_nslots - 1; i >= 0; i--) {
		slot = &sc->sc_slots[i];

		/* If already checked above, don't look again now. */
		if (slot->tcs_used)
			continue;

		/*
		 * Make sure something is there, and find out what it is.
		 */
		tcaddr = slot->tcs_addr;
		if (tc_badaddr(tcaddr))
			continue;
		if (tc_checkslot(tcaddr, tcdev.tcda_modname) == 0)
			continue;

		/*
		 * Set up the rest of the attachment information.
		 */
		tcdev.tcda_slot = i;
		tcdev.tcda_offset = 0;
		tcdev.tcda_addr = tcaddr;
		tcdev.tcda_cookie = slot->tcs_cookie;

		/*
		 * Mark the slot as used.
		 */
		slot->tcs_used = 1;

		/*
		 * Attach the device.
		 */
		config_found(self, &tcdev, tcprint);
	}
}

int
tcprint(aux, pnp)
	void *aux;
	char *pnp;
{
	struct tcdev_attach_args *tcdev = aux;

        if (pnp)
                printf("%s at %s", tcdev->tcda_modname, pnp);	/* XXX */
        printf(" slot %d offset 0x%lx", tcdev->tcda_slot,
	    (long)tcdev->tcda_offset);
        return (UNCONF);
}

int
tc_submatch(match, d)
	struct cfdata *match;
	struct tcdev_attach_args *d;
{

	return (((match->tccf_slot == d->tcda_slot) ||
		 (match->tccf_slot == TCCF_SLOT_UNKNOWN)) &&
		((match->tccf_offset == d->tcda_offset) ||
		 (match->tccf_offset == TCCF_OFFSET_UNKNOWN)));
}


#define	NTC_ROMOFFS	2
static tc_offset_t tc_slot_romoffs[NTC_ROMOFFS] = {
	TC_SLOT_ROM,
	TC_SLOT_PROTOROM,
};

int
tc_checkslot(slotbase, namep)
	tc_addr_t slotbase;
	char *namep;
{
	struct tc_rommap *romp;
	int i, j;

	for (i = 0; i < NTC_ROMOFFS; i++) {
		romp = (struct tc_rommap *)
		    (slotbase + tc_slot_romoffs[i]);

		switch (romp->tcr_width.v) {
		case 1:
		case 2:
		case 4:
			break;

		default:
			continue;
		}

		if (romp->tcr_stride.v != 4)
			continue;

		for (j = 0; j < 4; j++)
			if (romp->tcr_test[j+0*romp->tcr_stride.v] != 0x55 ||
			    romp->tcr_test[j+1*romp->tcr_stride.v] != 0x00 ||
			    romp->tcr_test[j+2*romp->tcr_stride.v] != 0xaa ||
			    romp->tcr_test[j+3*romp->tcr_stride.v] != 0xff)
				continue;

		for (j = 0; j < TC_ROM_LLEN; j++)
			namep[j] = romp->tcr_modname[j].v;
		namep[j] = '\0';
		return (1);
	}
	return (0);
}

void
tc_intr_establish(dev, cookie, level, handler, arg)
	struct device *dev;
	void *cookie, *arg;
	tc_intrlevel_t level;
	int (*handler) __P((void *));
{
	struct tc_softc *sc = (struct tc_softc *)dev;

	(*sc->sc_intr_establish)(sc->sc_dv.dv_parent, cookie, level, 
	    handler, arg);
}

void
tc_intr_disestablish(dev, cookie)
	struct device *dev;
	void *cookie;
{
	struct tc_softc *sc = (struct tc_softc *)dev;

	(*sc->sc_intr_disestablish)(sc->sc_dv.dv_parent, cookie);
}
