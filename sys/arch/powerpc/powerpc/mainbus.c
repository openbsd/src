/*	$OpenBSD: mainbus.c,v 1.2 1998/08/22 18:31:59 rahnds Exp $	*/

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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/autoconf.h>

struct mainbus_softc {
	struct	device sc_dv;
	struct	bushook sc_bus;
};

/* Definition of the mainbus driver. */
static int	mbmatch __P((struct device *, void *, void *));
static void	mbattach __P((struct device *, struct device *, void *));
static int	mbprint __P((void *, const char *));

struct cfattach mainbus_ca = {
	sizeof(struct device), mbmatch, mbattach
};
struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL, NULL, 0
};

void	mb_intr_establish __P((struct confargs *, int (*)(void *), void *));
void	mb_intr_disestablish __P((struct confargs *));
caddr_t	mb_cvtaddr __P((struct confargs *));
int	mb_matchname __P((struct confargs *, char *));

static int
mbmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;

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
	extern int system_type;

	printf("\n");

	sc->sc_bus.bh_dv = (struct device *)sc;
	sc->sc_bus.bh_type = BUS_MAIN;
	sc->sc_bus.bh_intr_establish = mb_intr_establish;
	sc->sc_bus.bh_intr_disestablish = mb_intr_disestablish;
	sc->sc_bus.bh_matchname = mb_matchname;

	/*
	 * Try to find and attach all of the CPUs in the machine.
	 * ( Right now only one CPU so code is simple )
	 */

	nca.ca_name = "cpu";
	nca.ca_bus = &sc->sc_bus;
	config_found(self, &nca, mbprint);

	/* Set up Openfirmware.*/
	if (system_type != POWER4e) { /* for now */
		nca.ca_name = "ofroot";
		nca.ca_bus = &sc->sc_bus;
		config_found(self, &nca, mbprint);
	} 


	/* The following machines have an ISA bus */
	/* Do ISA first so the interrupt controller is set up! */
	if (system_type == POWER4e) {
		nca.ca_name = "isabr";
		nca.ca_bus = &sc->sc_bus;
		config_found(self, &nca, mbprint);
	}

	/* The following machines have a PCI bus */
	if (system_type != OFWMACH) {
		nca.ca_name = "mpcpcibr";
		nca.ca_bus = &sc->sc_bus;
		config_found(self, &nca, mbprint);
	}
}

static int
mbprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	if (pnp)
		return (QUIET);
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
