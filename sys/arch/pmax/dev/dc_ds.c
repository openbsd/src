/*	$NetBSD: dc_ds.c,v 1.4 1996/10/14 17:28:46 jonathan Exp $	*/

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
#include <machine/dc7085cons.h>		/* XXX */
#include <pmax/dev/dcvar.h>
#include <pmax/dev/dc_ds_cons.h>
#include <pmax/pmax/kn01.h>

extern struct cfdriver mainbus_cd;	/* XXX */

/*
 * Autoconfig definition of driver front-end
 */
int	dc_ds_match  __P((struct device * parent, void *cfdata, void *aux));
void	dc_ds_attach __P((struct device *parent, struct device *self, void *aux));

struct cfattach dc_ds_ca = {
	sizeof(struct dc_softc), dc_ds_match, dc_ds_attach
};


/*
 * Initialize a line for (polled) console I/O
 */
int
dc_ds_consinit(dev)
	dev_t dev;
{
#if defined(DEBUG) && 1			/* XXX untested */
	printf("dc_ds(%d,%d): serial console at 0x%x\n",
	       minor(dev) >> 2, minor(dev) & 03,
	       MIPS_PHYS_TO_KSEG1(KN01_SYS_DZ));
#endif
	/* let any  pending PROM output from boot drain */
	DELAY(100000);
	dc_consinit(dev, (void *)MIPS_PHYS_TO_KSEG1(KN01_SYS_DZ));
	return (1);
}


/*
 * Match driver on decstation (2100,3100,5100) based on name
 */
int
dc_ds_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, "dc") != 0 &&
	    strcmp(ca->ca_name, "mdc") != 0 &&
	    strcmp(ca->ca_name, "dc7085") != 0)
		return (0);

	if (badaddr((caddr_t)ca->ca_addr, 2))
		return (0);

	return (1);
}


void
dc_ds_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct confargs *ca = aux;
	caddr_t dcaddr;
	struct dc_softc *sc = (void*) self;


	dcaddr = (caddr_t)ca->ca_addr;
	(void) dcattach(sc, (void*)MIPS_PHYS_TO_KSEG1(dcaddr),
			/* dtr/dsr mask: comm port only */
			1 << DCCOMM_PORT,
			/* rts/cts mask: none */
			0x0,
			0, DCCOMM_PORT);

	/* tie pseudo-slot to device */
	BUS_INTR_ESTABLISH(ca, dcintr, self);
	printf("\n");
}
