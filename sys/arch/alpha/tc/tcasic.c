/*	$NetBSD: tcasic.c,v 1.1 1995/12/20 00:43:34 cgd Exp $	*/

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

#include <dev/tc/tcvar.h>
#include <alpha/tc/tc_conf.h>

/* Definition of the driver for autoconfig. */
int	tcasicmatch(struct device *, void *, void *);
void	tcasicattach(struct device *, struct device *, void *);
struct cfdriver tcasiccd =
    { NULL, "tcasic", tcasicmatch, tcasicattach, DV_DULL,
    sizeof (struct device) };

int	tcasicprint __P((void *, char *));

extern int cputype;

/* There can be only one. */
int	tcasicfound;

int
tcasicmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;

        /* Make sure that we're looking for a TurboChannel ASIC. */
        if (strcmp(ca->ca_name, tcasiccd.cd_name))
                return (0);

        /* Make sure that the system supports a TurboChannel ASIC. */
	if ((cputype != ST_DEC_3000_500) && (cputype != ST_DEC_3000_300))
		return (0);

	if (tcasicfound)
		return (0);

	return (1);
}

void
tcasicattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct tc_attach_args tc;
	void (*intr_setup) __P((void));
	void (*iointr) __P((void *, int));

	tcasicfound = 1;

	switch (cputype) {
#ifdef DEC_3000_500
	case ST_DEC_3000_500:
		printf(": 25MHz\n");

		intr_setup = tc_3000_500_intr_setup;
		iointr = tc_3000_500_iointr;

		tc.tca_nslots = tc_3000_500_nslots;
		tc.tca_slots = tc_3000_500_slots;
		tc.tca_nbuiltins = tc_3000_500_nbuiltins;
		tc.tca_builtins = tc_3000_500_builtins;
		tc.tca_intr_establish = tc_3000_500_intr_establish;
		tc.tca_intr_disestablish = tc_3000_500_intr_disestablish;
		break;
#endif /* DEC_3000_500 */

#ifdef DEC_3000_300
	case ST_DEC_3000_300:
		printf(": 12.5MHz\n");

		intr_setup = tc_3000_300_intr_setup;
		iointr = tc_3000_300_iointr;

		tc.tca_nslots = tc_3000_300_nslots;
		tc.tca_slots = tc_3000_300_slots;
		tc.tca_nbuiltins = tc_3000_300_nbuiltins;
		tc.tca_builtins = tc_3000_300_builtins;
		tc.tca_intr_establish = tc_3000_300_intr_establish;
		tc.tca_intr_disestablish = tc_3000_300_intr_disestablish;
		break;
#endif /* DEC_3000_300 */

	default:
		printf("\n");
		panic("tcasicattach: bad cputype");
	}

	(*intr_setup)();
	set_iointr(iointr);

	config_found(self, &tc, tcasicprint);
}

int
tcasicprint(aux, pnp)
	void *aux;
	char *pnp;
{

	/* only TCs can attach to tcasics; easy. */
	if (pnp)
		printf("tc at %s", pnp);
	return (UNCONF);
}
