/*	$NetBSD: dc_ioasic.c,v 1.4 1996/10/14 17:15:42 jonathan Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * this driver contributed by Jonathan Stone
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <machine/autoconf.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>
#include <machine/dc7085cons.h>
#include <pmax/dev/dcvar.h>
#include <pmax/dev/dc_ioasic_cons.h>
#include <pmax/pmax/kn02.h>
/*
 * Autoconfig definition of driver front-end
 */
int	dc_ioasic_match  __P((struct device * parent, void *cfdata, void *aux));
void	dc_ioasic_attach __P((struct device *parent, struct device *self, void *aux));

struct cfattach dc_ioasic_ca = {
	sizeof(struct dc_softc), dc_ioasic_match, dc_ioasic_attach
};


/*
 * Initialize a line for (polled) console I/O
 */
int
dc_ioasic_consinit(dev)
	dev_t dev;
{

#if defined(DEBUG) && 0
	printf("dc_ioasic(%d,%d): serial console at 0x%x\n",
	       minor(dev) >> 2, minor(dev) & 03,
	       MIPS_PHYS_TO_KSEG1(KN02_SYS_DZ));
	DELAY(100000);
#endif
	dc_consinit(dev, (void *)MIPS_PHYS_TO_KSEG1(KN02_SYS_DZ));
	return(1);
}


/*
 * Match driver on 5000/200
 *  (not a real ioasic, but we configure it as one)
 */
int
dc_ioasic_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct ioasicdev_attach_args *d = aux;

	if (strcmp(d->iada_modname, "dc") != 0 &&
	    strcmp(d->iada_modname, "dc7085") != 0)
		return (0);

	if (badaddr((caddr_t)(d->iada_addr), 2))
		return (0);

	return (1);
}


void
dc_ioasic_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct dc_softc *sc = (void*) self;
	struct ioasicdev_attach_args *d = aux;
	caddr_t dcaddr;

	dcaddr = (caddr_t)d->iada_addr;
	printf(" address %p", dcaddr);
	(void) dcattach(sc, (void*)MIPS_PHYS_TO_KSEG1(dcaddr),
	/* dtr/dsr mask */	  (1<< DCPRINTER_PORT) + (1 << DCCOMM_PORT),
#ifdef HW_FLOW_CONTROL
	/* rts/cts mask */	  (1<< DCPRINTER_PORT) + (1 << DCCOMM_PORT),
#else
				  0,
#endif
				  1, 3);
	/* tie pseudo-slot to device */
	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_TTY,
			     dcintr, self);

	printf("\n");
}

