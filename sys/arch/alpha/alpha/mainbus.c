/*	$OpenBSD: mainbus.c,v 1.8 1997/01/24 19:56:38 niklas Exp $	*/
/*	$NetBSD: mainbus.c,v 1.15 1996/12/05 01:39:28 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>
#include <machine/cpuconf.h>

struct mainbus_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
};

/* Definition of the mainbus driver. */
#ifdef __BROKEN_INDIRECT_CONFIG
static int	mbmatch __P((struct device *, void *, void *));
#else
static int	mbmatch __P((struct device *, struct cfdata *, void *));
#endif
static void	mbattach __P((struct device *, struct device *, void *));
static int	mbprint __P((void *, const char *));

struct cfattach mainbus_ca = {
	sizeof(struct mainbus_softc), mbmatch, mbattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

void	mb_intr_establish __P((struct confargs *, int (*)(void *), void *));
void	mb_intr_disestablish __P((struct confargs *));
caddr_t	mb_cvtaddr __P((struct confargs *));
int	mb_matchname __P((struct confargs *, char *));

static int
#ifdef __BROKEN_INDIRECT_CONFIG
mbmatch(parent, cfdata, aux)
#else
mbmatch(parent, cf, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *cfdata;
#else
	struct cfdata *cf;
#endif
	void *aux;
{
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *cf = cfdata;
#endif

	/*
	 * Only one mainbus, but some people are stupid...
	 */	
	if (cf->cf_unit > 0)
		return(0);

	/*
	 * That one mainbus is always here.
	 */
	return(1);
}

static void
mbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	struct confargs nca;
	int i, cpuattachcnt;
	struct pcs* pcsp;
	extern int ncpus;
	extern const struct cpusw *cpu_fn_switch;

	printf("\n");

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_MAIN;
	sc->sc_bus.ab_intr_establish = mb_intr_establish;
	sc->sc_bus.ab_intr_disestablish = mb_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = mb_cvtaddr;
	sc->sc_bus.ab_matchname = mb_matchname;

	/*
	 * Try to find and attach all of the CPUs in the machine.
	 */
	cpuattachcnt = 0;
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		pcsp = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off +
		    (i * hwrpb->rpb_pcs_size));
		if ((pcsp->pcs_flags & PCS_PP) == 0)
			continue;

		nca.ca_name = "cpu";
		nca.ca_slot = 0;
		nca.ca_offset = 0;
		nca.ca_bus = &sc->sc_bus;
		if (config_found(self, &nca, mbprint) != NULL)
			cpuattachcnt++;
	}
	if (ncpus != cpuattachcnt)
		printf("WARNING: %d cpus in machine, %d attached\n",
			ncpus, cpuattachcnt);

	if ((*cpu_fn_switch->iobus_name)() != NULL) {
		char iobus_name[16];

		strncpy(iobus_name, (*cpu_fn_switch->iobus_name)(), 16);
		nca.ca_name = iobus_name;
		nca.ca_slot = 0;
		nca.ca_offset = 0;
		nca.ca_bus = &sc->sc_bus;
		config_found(self, &nca, mbprint);
	}
}

static int
mbprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct confargs *ca = aux;

	if (pnp)
		printf("%s at %s", ca->ca_name, pnp);

	return (UNCONF);
}

void
mb_intr_establish(ca, handler, val)
	struct confargs *ca;
	int (*handler) __P((void *));
	void *val;
{

	panic("can never mb_intr_establish");
}

void
mb_intr_disestablish(ca)
	struct confargs *ca;
{

	panic("can never mb_intr_disestablish");
}

caddr_t
mb_cvtaddr(ca)
	struct confargs *ca;
{

	return (NULL);
}

int
mb_matchname(ca, name)
	struct confargs *ca;
	char *name;
{

	return (strcmp(name, ca->ca_name) == 0);
}
