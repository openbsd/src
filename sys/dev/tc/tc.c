/*	$OpenBSD: tc.c,v 1.2 1996/04/18 23:48:22 niklas Exp $	*/
/*	$NetBSD: tc.c,v 1.10 1996/03/05 23:15:07 cgd Exp $	*/

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
#include <dev/tc/tcdevs.h>

struct tc_softc {
	struct	device sc_dv;

	int	sc_speed;
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
int	tcsubmatch __P((struct device *, void *, void *));
int	tc_checkslot __P((tc_addr_t, char *));
void	tc_devinfo __P((const char *, char *));

int
tcmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct tcbus_attach_args *tba = aux;

	if (strcmp(tba->tba_busname, cf->cf_driver->cd_name))
		return (0);

	/* XXX check other indicators */

	return (1);
}

void
tcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct tc_softc *sc = (struct tc_softc *)self;
	struct tcbus_attach_args *tba = aux;
	struct tc_attach_args ta;
	const struct tc_builtin *builtin;
	struct tc_slotdesc *slot;
	tc_addr_t tcaddr;
	void *match;
	int i;

	printf("%s MHz clock\n",
	    tba->tba_speed == TC_SPEED_25_MHZ ? "25" : "12.5");

	/*
	 * Save important CPU/chipset information.
	 */
	sc->sc_speed = tba->tba_speed;
	sc->sc_nslots = tba->tba_nslots;
	sc->sc_slots = tba->tba_slots;
	sc->sc_intr_establish = tba->tba_intr_establish;
	sc->sc_intr_disestablish = tba->tba_intr_disestablish;

	/*
	 * Try to configure each built-in device
	 */
	for (i = 0; i < tba->tba_nbuiltins; i++) {
		builtin = &tba->tba_builtins[i];

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
		strncpy(ta.ta_modname, builtin->tcb_modname, TC_ROM_LLEN);
		ta.ta_modname[TC_ROM_LLEN] = '\0';
		ta.ta_slot = builtin->tcb_slot;
		ta.ta_offset = builtin->tcb_offset;
		ta.ta_addr = tcaddr;
		ta.ta_cookie = builtin->tcb_cookie;
		ta.ta_busspeed = sc->sc_speed;
	
		/*
		 * Mark the slot as used, so we don't check it later.
		 */
		sc->sc_slots[builtin->tcb_slot].tcs_used = 1;

		/*
		 * Attach the device.
		 */
		config_found_sm(self, &ta, tcprint, tcsubmatch);
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
		if (tc_checkslot(tcaddr, ta.ta_modname) == 0)
			continue;

		/*
		 * Set up the rest of the attachment information.
		 */
		ta.ta_slot = i;
		ta.ta_offset = 0;
		ta.ta_addr = tcaddr;
		ta.ta_cookie = slot->tcs_cookie;

		/*
		 * Mark the slot as used.
		 */
		slot->tcs_used = 1;

		/*
		 * Attach the device.
		 */
		config_found_sm(self, &ta, tcprint, tcsubmatch);
	}
}

int
tcprint(aux, pnp)
	void *aux;
	char *pnp;
{
	struct tc_attach_args *ta = aux;
	char devinfo[256];

	if (pnp) {
		tc_devinfo(ta->ta_modname, devinfo);
		printf("%s at %s", devinfo, pnp);  
	}
	printf(" slot %d offset 0x%lx", ta->ta_slot,
	    (long)ta->ta_offset);
	return (UNCONF);
}

int   
tcsubmatch(parent, match, aux) 
        struct device *parent;
        void *match, *aux; 
{
	struct cfdata *cf = match;
	struct tc_attach_args *d = aux;

	if ((cf->tccf_slot != TCCF_SLOT_UNKNOWN) &&
	    (cf->tccf_slot != d->ta_slot))
		return 0;
	if ((cf->tccf_offset != TCCF_SLOT_UNKNOWN) &&
	    (cf->tccf_offset != d->ta_offset))
		return 0;

	return ((*cf->cf_driver->cd_match)(parent, match, aux));
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

#ifdef TCVERBOSE
/*      
 * Descriptions of of known devices.
 */     
struct tc_knowndev {
	const char *id, *driver, *description;
};      

#include <dev/tc/tcdevs_data.h>
#endif /* TCVERBOSE */

void
tc_devinfo(id, cp)
	const char *id;
	char *cp;
{
	const char *driver, *description;
#ifdef TCVERBOSE
	struct tc_knowndev *tdp;
	int match;
	const char *unmatched = "unknown ";
#else
	const char *unmatched = "";
#endif

	driver = NULL;
	description = id;

#ifdef TCVERBOSE
	/* find the device in the table, if possible. */
	tdp = tc_knowndevs;
	while (tdp->id != NULL) {
		/* check this entry for a match */
		match = !strcmp(tdp->id, id);
		if (match) {
			driver = tdp->driver;
			description = tdp->description;
			break;
		}
		tdp++;
	}
#endif

	if (driver == NULL)
		cp += sprintf(cp, "%sdevice %s", unmatched, id);
	else
		cp += sprintf(cp, "%s (%s)", driver, description);
}
