/*	$OpenBSD: mainbus.c,v 1.10 2004/11/18 16:10:43 miod Exp $	*/

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

void	mbattach(struct device *, struct device *, void *);
int	mbmatch(struct device *, void *, void *);
int	mbprint(void *, const char *);

struct cfattach mainbus_ca = {
	sizeof(struct mainbus_softc), mbmatch, mbattach
};
struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int
mbmatch(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

void
mbattach(struct device *parent, struct device *self, void *aux)
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	struct confargs nca;
	u_int8_t systype;
	int cpu;
	extern vaddr_t isaspace_va;

	/* Pretty print the system type */
	printf(": ");
	switch ((systype = *(u_int8_t *)(isaspace_va + MVME_STATUS_REG))) {
	default:
		printf("unknown system type %x", systype);
		break;
	case MVMETYPE_RESERVED:
		/* if you ever have this one, please contact me -- miod */
		printf("Dahu MVME");
		break;
	case MVMETYPE_2600_712:
		cpu = ppc_mfpvr() >> 16;
		if (cpu == PPC_CPU_MPC750)
			printf("MVME2700 (712-compatible)");
		else
			printf("MVME2600 (712-compatible)");
		break;
	case MVMETYPE_2600_761:
		cpu = ppc_mfpvr() >> 16;
		if (cpu == PPC_CPU_MPC750)
			printf("MVME2700 (761-compatible)");
		else
			printf("MVME2600 (761-compatible)");
		break;
	case MVMETYPE_3600_712:
		printf("MVME3600 or MVME4600 (712-compatible)");
		break;
	case MVMETYPE_3600_761:
		printf("MVME3600 or MVME4600 (761-compatible)");
		break;
	case MVMETYPE_1600:
		printf("MVME1600");
		break;
	}
	printf("\n");

	sc->sc_bus.bh_dv = (struct device *)sc;
	sc->sc_bus.bh_type = BUS_MAIN;
	sc->sc_bus.bh_intr_establish = NULL;
	sc->sc_bus.bh_intr_disestablish = NULL;
	sc->sc_bus.bh_matchname = NULL;

	/*
	 * Try to find and attach all of the CPUs in the machine.
	 * Right now only one CPU is supported, so this is simple.
	 * Need to change for real MVME4600 support.
	 */

	nca.ca_name = "cpu";
	nca.ca_bus = &sc->sc_bus;
	config_found(self, &nca, mbprint);

	/*
	 * Attach the BUG terminal services if necessary.
	 */
	nca.ca_name = "bugtty";
	nca.ca_bus = &sc->sc_bus;
	config_found(self, &nca, mbprint);

	/*
	 * Find and attach the PCI Northbridge. It will find and attach
	 * everything.
	 */
	nca.ca_name = "raven";
	nca.ca_bus = &sc->sc_bus;
	config_found(self, &nca, mbprint);
}

int
mbprint(void *aux, const char *pnp)
{
	if (pnp)
		return (QUIET);
	return (UNCONF);
}
