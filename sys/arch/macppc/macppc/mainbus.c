/*	$OpenBSD: mainbus.c,v 1.13 2005/10/19 14:46:00 kettenis Exp $	*/

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
#include <dev/ofw/openfirm.h>

struct mainbus_softc {
	struct	device sc_dv;
	struct	bushook sc_bus;
};

/* Definition of the mainbus driver. */
static int	mbmatch(struct device *, void *, void *);
static void	mbattach(struct device *, struct device *, void *);
static int	mbprint(void *, const char *);

struct cfattach mainbus_ca = {
	sizeof(struct mainbus_softc), mbmatch, mbattach
};
struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

void	mb_intr_establish(struct confargs *, int (*)(void *), void *);
void	mb_intr_disestablish(struct confargs *);
caddr_t	mb_cvtaddr(struct confargs *);
int	mb_matchname(struct confargs *, char *);

/*ARGSUSED*/
static int
mbmatch(struct device *parent, void *cfdata, void *aux)
{

	/*
	 * That one mainbus is always here.
	 */
	return(1);
}

static void
mbattach(struct device *parent, struct device *self, void *aux)
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	struct confargs nca;
	char name[32];
	int node;

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
	{ /* legacy? */
		nca.ca_name = "ofroot";
		nca.ca_bus = &sc->sc_bus;
		config_found(self, &nca, mbprint);
	}

	for (node = OF_child(OF_peer(0)); node; node=OF_peer(node)) {
		bzero (name, sizeof(name));
		if (OF_getprop(node, "device_type", name,
		    sizeof(name)) <= 0) {
			if (OF_getprop(node, "name", name,
			    sizeof(name)) <= 0)
				printf ("name not found on node %x\n",
				    node);
				continue;
		}
		if (strcmp(name, "memory-controller") == 0) {
			nca.ca_name = "memc";
			nca.ca_node = node;
			nca.ca_bus = &sc->sc_bus;
			config_found(self, &nca, mbprint);
		}
		if (strcmp(name, "pci") == 0) {
			nca.ca_name = "mpcpcibr";
			nca.ca_node = node;
			nca.ca_bus = &sc->sc_bus;
			config_found(self, &nca, mbprint);
		}
		if (strcmp(name, "ht") == 0) {
			nca.ca_name = "ht";
			nca.ca_node = node;
			nca.ca_bus = &sc->sc_bus;
			config_found(self, &nca, mbprint);
		}
		if (strcmp(name, "smu") == 0) {
			nca.ca_name = "smu";
			nca.ca_node = node;
			nca.ca_bus = &sc->sc_bus;
			config_found(self, &nca, mbprint);
		}
	}
}

static int
mbprint(void *aux, const char *pnp)
{
	if (pnp)
		return (QUIET);
	return (UNCONF);
}

void
mb_intr_establish(struct confargs *ca, int (*handler)(void *), void *val)
{
	panic("can never mb_intr_establish");
}

void
mb_intr_disestablish(struct confargs *ca)
{
	panic("can never mb_intr_disestablish");
}

caddr_t
mb_cvtaddr(struct confargs *ca)
{
	return (NULL);
}

int
mb_matchname(struct confargs *ca, char *name)
{
	return (strcmp(name, ca->ca_name) == 0);
}
