/*	$NetBSD: sii_ds.c,v 1.2 1996/10/13 16:59:15 christos Exp $	*/

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
#include <pmax/dev/device.h>		/* XXX old pmax SCSI drivers */
#include <pmax/dev/siireg.h>
#include <pmax/dev/siivar.h>

#include <pmax/pmax/kn01.h>		/* kn01 (ds3100) address constants */

extern struct cfdriver mainbus_cd;	/* XXX */

/*
 * Autoconfig definition of driver front-end
 */
int	sii_ds_match  __P((struct device * parent, void *cfdata, void *aux));
void	sii_ds_attach __P((struct device *parent, struct device *self, void *aux));


extern struct cfattach sii_ds_ca;
struct cfattach sii_ds_ca = {
	sizeof(struct siisoftc), sii_ds_match, sii_ds_attach
};


/* define a safe address in the SCSI buffer for doing status & message DMA */
#define SII_BUF_ADDR	(MIPS_PHYS_TO_KSEG1(KN01_SYS_SII_B_START) \
		+ SII_MAX_DMA_XFER_LENGTH * 14)

/*
 * Match driver on Decstation (2100, 3100, 5100) based on name and probe.
 */
int
sii_ds_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct confargs *ca = aux;
	register void * siiaddr;

	if (strcmp(ca->ca_name, "sii") != 0 &&
	    strncmp(ca->ca_name, "PMAZ-AA ", 8) != 0) /*XXX*/
		return (0);

	/* XXX check for bad address, untested */
	siiaddr = (void *)ca->ca_addr;
	if (siiaddr != (void *)MIPS_PHYS_TO_KSEG1(KN01_SYS_SII)) {
		printf("(siimatch: bad addr %x, substituting %x\n",
			ca->ca_addr, MIPS_PHYS_TO_KSEG1(KN01_SYS_SII));
		siiaddr = (void *)MIPS_PHYS_TO_KSEG1(KN01_SYS_SII);
	}
	if (badaddr(siiaddr, 4))
		return (0);
	return (1);
}

void
sii_ds_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct confargs *ca = aux;
	register struct siisoftc *sc = (struct siisoftc *) self;

	sc->sc_regs = (SIIRegs *)MIPS_PHYS_TO_KSEG1(ca->ca_addr);

	/* set up scsi buffer.  XXX Why statically allocated? */
	sc->sc_buf = (void*)(MIPS_PHYS_TO_KSEG1(KN01_SYS_SII_B_START));

siiattach(sc);

	/* tie pseudo-slot to device */
	BUS_INTR_ESTABLISH(ca, siiintr, sc);
	printf("\n");
}
