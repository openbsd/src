/*	$NetBSD: mainbus.c,v 1.18 1996/10/13 03:39:51 christos Exp $	*/

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
#include <machine/machConst.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>

#include "pmaxtype.h"
#include "nameglue.h"

#include "kn01.h"
#include <pmax/pmax/kn01var.h>

#include "tc.h"			/* Is Turbochannel configured? */

struct mainbus_softc {
	struct	device sc_dv;
};

/* Definition of the mainbus driver. */
static int	mbmatch __P((struct device *, void *, void *));
static void	mbattach __P((struct device *, struct device *, void *));
static int	mbprint __P((void *, const char *));

struct cfattach mainbus_ca = {
	sizeof (struct mainbus_softc), mbmatch, mbattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

void	mb_intr_establish __P((struct confargs *ca,
			       int (*handler)(intr_arg_t),
			       intr_arg_t val ));
void	mb_intr_disestablish __P((struct confargs *));

/*
 * Declarations of Potential child busses and how to configure them.
 */
/* KN01 has devices directly on the system bus */
void	kn01_intr_establish __P((struct device *parent, void *cookie,
				 int level,
			       int (*handler)(intr_arg_t),
			       intr_arg_t val ));
void	kn01_intr_disestablish __P((struct confargs *));
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
	register struct device *mb = self;
	struct confargs nca;

	extern int cputype, ncpus;

	printf("\n");

	/*
	 * if we ever support multi-CPU DEcstations (5800 family),
	 * the Alpha port's mainbus.c has an example of attaching
	 * multiple CPUs.
	 *
	 * For now, we only have one. Attach it directly.
	 */
	/*nca.ca_name = "cpu";*/
	bcopy("cpu", nca.ca_name, sizeof(nca.ca_name));
	nca.ca_slot = 0;
	nca.ca_offset = 0;
	nca.ca_addr = 0;
	config_found(mb, &nca, mbprint);

#if NTC > 0
	if (cputype == DS_3MAXPLUS || cputype == DS_3MAX ||
	    cputype == DS_3MIN || cputype == DS_MAXINE) {
		/*
		 * This system might have a turbochannel.
		 * Call the TC subr code to look for one
		 * and if found, to configure it.
		 */
		config_tcbus(mb, 0 /* XXX */, mbprint);
	}
#endif /* NTC */

	/*
	 * We haven't yet decided how to handle the PMAX (KN01)
	 * which really only has a mainbus, baseboard devices, and an
	 * optional framebuffer.
	 */
#if 1 /*defined(DS3100)*/

	/* XXX mipsmate: just a guess */
	if (cputype == DS_PMAX || cputype == DS_MIPSMATE) {
		kn01_attach(mb, (void*)0, aux);
	}
#endif /*DS3100*/
}


#define KN01_MAXDEVS 9
struct confargs kn01_devs[KN01_MAXDEVS] = {
	/*   name       slot  offset 		   addr intpri  */
	{ "pm",		0,   0,  (u_int)KV(KN01_PHYS_FBUF_START), 3,  },
	{ "dc",  	1,   0,  (u_int)KV(KN01_SYS_DZ),	  2,  },
	{ "lance", 	2,   0,  (u_int)KV(KN01_SYS_LANCE),       1,  },
	{ "sii",	3,   0,  (u_int)KV(KN01_SYS_SII),	  0,  },
	{ "mc146818",	4,   0,  (u_int)KV(KN01_SYS_CLOCK),       16, },
	{ "dc",  	5,   0,  (u_int)KV(0x15000000),	  	  4,  },
	{ "dc",  	6,   0,  (u_int)KV(0x15200000),	 	  5,  },
	{ "led",	6,   0,  0,				  -1, },
#ifdef notyet
	/*
	 * XXX Ultrix configures at 0x86400400. the first 0x400 byte are
	 * used for NVRAM state??
	 */
	{ "nvram",	6,   0,  (u_int)KV(0x86400000),	  -1, },
#endif
	{ "", 0, 0 }
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
	struct confargs *nca;
	register int i;

#ifdef DEBUG
/*XXX*/ printf("(configuring kn01/5100 baseboard devices)\n");
#endif

	/* Try to configure each KN01 mainbus device */
	for (i = 0; i < KN01_MAXDEVS; i++) {

		nca = &kn01_devs[i];
		if (nca == NULL) {
			printf("mbattach: bad config for slot %d\n", i);
			break;
		}

		if (nca->ca_name == NULL) {
			panic("No name for mainbus device");
		}

#if defined(DIAGNOSTIC) || defined(DEBUG)
		if (nca->ca_slot > KN01_MAXDEVS)
			panic("kn01 mbattach: \
dev slot > number of slots for %s",
			    nca->ca_name);
#endif

#ifdef DEBUG
		printf("configuring %s at %x interrupt number %d\n",
		       nca->ca_name, nca->ca_addr, (u_int)nca->ca_slotpri);
#endif

		/* Tell the autoconfig machinery we've found the hardware. */
		config_found(parent, nca, mbprint);
	}

	/*
	 * The Decstation 5100, like the 3100, has an sii, clock, ethernet,
	 * and dc, presumably at the same addresses.   If so, the
	 * code above will configure them.  The  5100 also
	 * has a slot for PrestoServe NVRAM and for an additional
	 * `mdc' dc-like, eigh-port serial option. If we supported
	 * those devices, this is the right place to configure them.
	 */
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

#ifdef DS3100
void
kn01_intr_establish(parent, cookie, level, handler, arg)
	struct device *parent;
	void * cookie;
	int level;
	int (*handler) __P((intr_arg_t));
	intr_arg_t arg;
{
	/* Interrupts on the KN01 are currently hardcoded. */
	printf(" (kn01: intr_establish hardcoded) ");
	kn01_enable_intr((u_int) cookie, handler, arg, 1);
}

void
kn01_intr_disestablish(ca)
	struct confargs *ca;
{
	printf("(kn01: ignoring intr_disestablish) ");
}
#endif /* DS3100 */

/*
 * An  interrupt-establish method.  This should somehow be folded
 * back into the autoconfiguration machinery. Until the TC machine
 * portmasters agree on how to do that, it's a separate function.
 *
 * XXX since all drivers should be passign a softc for "arg",
 * why not make that explicit and use (struct device*)arg->dv_parent,
 * instead of explicitly passign the parent?
*/
void
generic_intr_establish(parent, cookie, level, handler, arg)
	void * parent;
	void * cookie;
	int level;
	intr_handler_t handler;
	intr_arg_t arg;
{
	struct device *dev = arg;

#if NTC>0
	extern struct cfdriver ioasic_cd, tc_cd;

	if (dev->dv_parent->dv_cfdata->cf_driver == &ioasic_cd) {
		/*XXX*/ printf("ioasic interrupt for %d\n", (u_int)cookie);
		ioasic_intr_establish(parent, cookie, level, handler, arg);
	} else
	if (dev->dv_parent->dv_cfdata->cf_driver == &tc_cd) {
		tc_intr_establish(parent, cookie, level, handler, arg);
	} else
#endif
#ifdef DS3100
	if (dev->dv_parent->dv_cfdata->cf_driver == &mainbus_cd) {
		kn01_intr_establish(parent, cookie, level, handler, arg);
	}
	else {
#else
	{
#endif
		printf("intr_establish: unknown parent bustype for %s\n",
			dev->dv_xname);
	}
}
