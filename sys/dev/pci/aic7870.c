/*	$NetBSD: aic7870.c,v 1.8 1996/03/17 00:55:23 thorpej Exp $	*/

/*
 * Product specific probe and attach routines for:
 *      294X and aic7870 motherboard SCSI controllers
 *
 * Copyright (c) 1995 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Justin T. Gibbs.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/aic7xxxvar.h>


#define PCI_BASEADR0		PCI_MAPREG_START
#define PCI_VENDORID(x)         ((x) & 0xFFFF)
#define PCI_CHIPID(x)           (((x) >> 16) & 0xFFFF)

static int aic7870_probe __P((struct device *, void *, void *));
static void aic7870_attach __P((struct device *, struct device *, void *));

struct cfattach ahc_ca = {
	sizeof(struct ahc_softc), aic7870_probe, aic7870_attach
};

struct cfdriver ahc_cd = {
        NULL, "ahc", DV_DULL
}; 

int ahcintr __P((void *));
      

int
aic7870_probe(parent, match, aux)
        struct device *parent;
        void *match, *aux; 
{       
        struct pci_attach_args *pa = aux;

	if (PCI_VENDORID(pa->pa_id) != PCI_VENDOR_ADP)
		return 0;

	switch (PCI_CHIPID(pa->pa_id)) {
	case PCI_PRODUCT_ADP_AIC7870:
	case PCI_PRODUCT_ADP_2940:
	case PCI_PRODUCT_ADP_2940U:
		return 1;
	default:
		return 0;
	}
}

void    
aic7870_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{       
        struct pci_attach_args *pa = aux;
        struct ahc_softc *ahc = (void *)self;
	int iobase; 

	switch (PCI_CHIPID(pa->pa_id)) {
	case PCI_PRODUCT_ADP_AIC7870:
		ahc->type = AHC_AIC7870;
		break;

	case PCI_PRODUCT_ADP_2940:
	case PCI_PRODUCT_ADP_2940U:
		ahc->type = AHC_294;
		break;
	}

	if (pci_map_io(pa->pa_tag, PCI_BASEADR0, &iobase))
		return;

	/*
	 * Make the offsets the same as for EISA
	 */
	iobase -= 0xc00ul; 

	if (ahcprobe(ahc, iobase) == 0)
		return;

	ahcattach(ahc);

	ahc->sc_ih = pci_map_int(pa->pa_tag, IPL_BIO, ahcintr, ahc);
}
