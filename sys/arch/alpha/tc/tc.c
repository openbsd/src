/*	$NetBSD: tc.c,v 1.2 1995/03/08 00:39:05 cgd Exp $	*/

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
#include <machine/rpb.h>

#include <alpha/tc/tc.h>

struct tc_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
	struct	tc_cpu_desc *sc_desc;
};

/* Definition of the driver for autoconfig. */
int	tcmatch(struct device *, void *, void *);
void	tcattach(struct device *, struct device *, void *);
int	tcprint(void *, char *);
struct cfdriver tccd =
    { NULL, "tc", tcmatch, tcattach, DV_DULL, sizeof (struct tc_softc) };

void	tc_intr_establish __P((struct confargs *, int (*)(void *), void *));
void	tc_intr_disestablish __P((struct confargs *));
caddr_t	tc_cvtaddr __P((struct confargs *));
int	tc_matchname __P((struct confargs *, char *));

extern int cputype;

#ifdef DEC_3000_300
extern struct tc_cpu_desc	dec_3000_300_cpu;
#endif
#ifdef DEC_3000_500
extern struct tc_cpu_desc	dec_3000_500_cpu;
#endif

struct tc_cpu_desc *tc_cpu_devs[] = {
        NULL,                   /* Unused */
        NULL,                   /* ST_ADU */
        NULL,                   /* ST_DEC_4000 */
        NULL,                   /* ST_DEC_7000 */
#ifdef DEC_3000_500
        &dec_3000_500_cpu,      /* ST_DEC_3000_500 */
#else
        NULL,
#endif
        NULL,                   /* Unused  */
        NULL,                   /* ST_DEC_2000_300 */
#ifdef DEC_3000_300
        &dec_3000_300_cpu,      /* ST_DEC_3000_300 */
#else
        NULL,
#endif
};
int ntc_cpu_devs = sizeof tc_cpu_devs / sizeof tc_cpu_devs[0];

int
tcmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

        /* Make sure that we're looking for a TC. */
        if (strcmp(ca->ca_name, tccd.cd_name) != 0)
                return (0);

        /* Make sure that unit exists. */
	if (cf->cf_unit != 0 ||
	    cputype > ntc_cpu_devs || tc_cpu_devs[cputype] == NULL)
		return (0);

	return (1);
}

void
tcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct tc_softc *sc = (struct tc_softc *)self;
	struct confargs *nca;
	char namebuf[TC_ROM_LLEN+1];
	int i;

	printf("\n");

	/* keep our CPU description handy */
	sc->sc_desc = tc_cpu_devs[cputype];

	/* set up interrupt handlers */
	(*sc->sc_desc->tc_intr_setup)();
	set_iointr(sc->sc_desc->tc_iointr);

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_TC;
	sc->sc_bus.ab_intr_establish = tc_intr_establish;
	sc->sc_bus.ab_intr_disestablish = tc_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = tc_cvtaddr;
	sc->sc_bus.ab_matchname = tc_matchname;

	/* Try to configure each CPU-internal device */
	for (i = 0; i < sc->sc_desc->tcd_ndevs; i++) {
		nca = &sc->sc_desc->tcd_devs[i];
		nca->ca_bus = &sc->sc_bus;

#ifdef DIAGNOSTIC
		if (nca->ca_slot > sc->sc_desc->tcd_nslots)
			panic("tcattach: dev slot > number of slots for %s",
			    nca->ca_name);
#endif

		if (tc_checkdevmem(nca) == 0)
			continue;

		/* If no name, we have to see what might be there. */
		if (nca->ca_name == NULL) {
			if (tc_checkslot(nca, namebuf) == 0)
				continue;
			nca->ca_name = namebuf;
		}

		/* Tell the autoconfig machinery we've found the hardware. */
		config_found(self, nca, tcprint);
	}
}

int
tcprint(aux, pnp)
	void *aux;
	char *pnp;
{
	struct confargs *ca = aux;

        if (pnp)
                printf("%s at %s", ca->ca_name, pnp);
        printf(" slot %ld offset 0x%lx", ca->ca_slot, ca->ca_offset);
        return (UNCONF);
}

caddr_t
tc_cvtaddr(ca)
	struct confargs *ca;
{
	struct tc_softc *sc = tccd.cd_devs[0];

	return (sc->sc_desc->tcd_slots[ca->ca_slot].tsd_dense + ca->ca_offset);

}

void
tc_intr_establish(ca, handler, val)
	struct confargs *ca;
	intr_handler_t handler;
	void *val;
{
	struct tc_softc *sc = tccd.cd_devs[0];

	(*sc->sc_desc->tc_intr_establish)(ca, handler, val);
}

void
tc_intr_disestablish(ca)
	struct confargs *ca;
{
	struct tc_softc *sc = tccd.cd_devs[0];

	(*sc->sc_desc->tc_intr_disestablish)(ca);
}

int
tc_matchname(ca, name)
	struct confargs *ca;
	char *name;
{

	return (bcmp(name, ca->ca_name, TC_ROM_LLEN) == 0);
}

int
tc_checkdevmem(ca)
	struct confargs *ca;
{
	u_int32_t *datap;

	datap = (u_int32_t *)BUS_CVTADDR(ca);

	/* Return non-zero if memory was there (i.e. address wasn't bad). */
	return (!badaddr(datap, sizeof (u_int32_t)));
}

u_int64_t tc_slot_romoffs[] = { TC_SLOT_ROM, TC_SLOT_PROTOROM };
int ntc_slot_romoffs = sizeof tc_slot_romoffs / sizeof tc_slot_romoffs[0];

int
tc_checkslot(ca, namep)
	struct confargs *ca;
	char *namep;
{
	struct tc_rommap *romp;
	int i, j;

	for (i = 0; i < ntc_slot_romoffs; i++) {
		romp = (struct tc_rommap *)
		    (BUS_CVTADDR(ca) + tc_slot_romoffs[i]);

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
		namep[TC_ROM_LLEN] = '\0';
		return (1);
	}
	return (0);
}

int
tc_intrnull(val)
	void *val;
{

	panic("uncaught TC intr for slot %ld\n", (long)val);
}
