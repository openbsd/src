/*	$NetBSD: mainbus.c,v 1.4 1995/12/28 06:45:01 jonathan Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * DECstation port: Jonathan Stone
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
/*#include <machine/rpb.h>*/
#include "pmaxtype.h"
#include "machine/machConst.h"
#include "nameglue.h"
#include "kn01.h"

struct mainbus_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
};

/* Definition of the mainbus driver. */
static int	mbmatch __P((struct device *, void *, void *));
static void	mbattach __P((struct device *, struct device *, void *));
static int	mbprint __P((void *, char *));
struct cfdriver mainbuscd =
    { NULL, "mainbus", mbmatch, mbattach, DV_DULL,
	sizeof (struct mainbus_softc) };
void	mb_intr_establish __P((struct confargs *ca,
			       int (*handler)(intr_arg_t),
			       intr_arg_t val ));
void	mb_intr_disestablish __P((struct confargs *));
caddr_t	mb_cvtaddr __P((struct confargs *));
int	mb_matchname __P((struct confargs *, char *));

/* KN01 has devices directly on the system bus */
void	kn01_intr_establish __P((struct confargs *ca,
			       int (*handler)(intr_arg_t),
			       intr_arg_t val ));
void	kn01_intr_disestablish __P((struct confargs *));
caddr_t	kn01_cvtaddr __P((struct confargs *));
static void	kn01_attach __P((struct device *, struct device *, void *));


static int
mbmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;

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

int ncpus = 0;	/* only support uniprocessors, for now */

static void
mbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	struct confargs nca;
	struct pcs *pcsp;
	int i, cpuattachcnt;
	extern int cputype, ncpus;

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

#ifdef notyet	/* alpha code */
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		struct pcs *pcsp;

		pcsp = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off +
		    (i * hwrpb->rpb_pcs_size));
		if ((pcsp->pcs_flags & PCS_PP) == 0)
			continue;

		nca.ca_name = "cpu";
		nca.ca_slot = 0;
		nca.ca_offset = 0;
		nca.ca_bus = &sc->sc_bus;
		if (config_found(self, &nca, mbprint))
			cpuattachcnt++;
	}
#endif

	if (ncpus != cpuattachcnt)
		printf("WARNING: %d cpus in machine, %d attached\n",
			ncpus, cpuattachcnt);

#if	defined(DS_5000) || defined(DS5000_240) || defined(DS_5000_100) || \
	defined(DS_5000_25)

	if (cputype == DS_3MAXPLUS ||
	    cputype == DS_3MAX ||
	    cputype == DS_3MIN ||
	    cputype == DS_MAXINE) {

	    if (cputype == DS_3MIN || cputype == DS_MAXINE)
		printf("UNTESTED autoconfiguration!!\n");	/*XXX*/

		/* we have a TurboChannel bus! */
		nca.ca_name = "tc";
		nca.ca_slot = 0;
		nca.ca_offset = 0;
		nca.ca_bus = &sc->sc_bus;
		config_found(self, &nca, mbprint);
	}
#endif /*Turbochannel*/

	/*
	 * We haven't yet decided how to handle the PMAX (KN01)
	 * which really only has a mainbus, baseboard devices, and an
	 * optional framebuffer.
	 */
#if defined(DS3100)
	/* XXX mipsfair: just a guess */
	if (cputype == DS_PMAX || cputype == DS_MIPSFAIR) {
		/*XXX*/
		sc->sc_bus.ab_intr_establish = kn01_intr_establish;
		sc->sc_bus.ab_intr_disestablish = kn01_intr_disestablish;
		sc->sc_bus.ab_cvtaddr = kn01_cvtaddr;

		kn01_attach(parent, self, aux);
	}
#endif /*DS3100*/

}


#define KN01_MAXDEVS 5
struct confargs kn01_devs[KN01_MAXDEVS] = {
	/* name	      index   pri xxx */
	{ "pm",		0,    3,   (u_int)KV(KN01_PHYS_FBUF_START) },
	{ "dc",  	1,    2,   (u_int)KV(KN01_SYS_DZ)	},
	{ "lance", 	2,    1,   (u_int)KV(KN01_SYS_LANCE)	},
	{ "sii",	3,    0,   (u_int)KV(KN01_SYS_SII)	},
	{ "dallas_rtc",	4,    16,  (u_int)KV(KN01_SYS_CLOCK)	}
};

/*
 * Configure baseboard devices on KN01 attached directly to mainbus 
 */
void
kn01_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	struct confargs *nca;
	register int i;

	/* Try to configure each KN01 mainbus device */
	for (i = 0; i < KN01_MAXDEVS; i++) {

		nca = &kn01_devs[i];
		if (nca == NULL) {
			printf("mbattach: bad config for slot %d\n", i);
			break;
		}
		nca->ca_bus = &sc->sc_bus;

#if defined(DIAGNOSTIC) || defined(DEBUG)
		if (nca->ca_slot > KN01_MAXDEVS)
			panic("kn01 mbattach: \
dev slot > number of slots for %s",
			    nca->ca_name);
#endif

		if (nca->ca_name == NULL) {
			panic("No name for mainbus device\n");
		}

		/* Tell the autoconfig machinery we've found the hardware. */
		config_found(self, nca, mbprint);
	}
}

static int
mbprint(aux, pnp)
	void *aux;
	char *pnp;
{

	if (pnp)
		return (QUIET);
	return (UNCONF);
}

void
mb_intr_establish(ca, handler, val)
	struct confargs *ca;
	int (*handler) __P((intr_arg_t));
	intr_arg_t val;
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

void
kn01_intr_establish(ca, handler, val)
	struct confargs *ca;
	int (*handler) __P((intr_arg_t));
	intr_arg_t val;
{
	/* Interrupts on the KN01 are currently hardcoded. */
	printf(" (kn01: intr_establish hardcoded) ");
}

void
kn01_intr_disestablish(ca)
	struct confargs *ca;
{
	printf("(kn01: ignoring intr_disestablish) ");
}

caddr_t
kn01_cvtaddr(ca)
	struct confargs *ca;
{
	return ((void *)ca->ca_offset);
}
