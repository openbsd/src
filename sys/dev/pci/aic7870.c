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
 *
 *	$Id: aic7870.c,v 1.1.1.1 1995/10/18 08:52:39 deraadt Exp $
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
#include <dev/ic/aic7xxxvar.h>


#define PCI_BASEADR0	PCI_MAP_REG_START
#define PCI_DEVICE_ID_ADAPTEC_2940	0x71789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7870	0x70789004ul


static int aic7870_probe();
static void aic7870_attach();

struct cfdriver ahccd = {
        NULL, "ahc", aic7870_probe, aic7870_attach, DV_DULL, 
        sizeof(struct ahc_softc)
}; 

int ahcintr __P((void *));
      

int
aic7870_probe(parent, match, aux)
        struct device *parent;
        void *match, *aux; 
{       
        struct pci_attach_args *pa = aux;

	if (pa->pa_id != PCI_DEVICE_ID_ADAPTEC_2940 &&
	    pa->pa_id != PCI_DEVICE_ID_ADAPTEC_AIC7870)
		return (0);

	return (1);
}

void    
aic7870_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{       
        struct pci_attach_args *pa = aux;
        struct ahc_softc *ahc = (void *)self;
	int iobase; 

        switch (pa->pa_id) {
        case PCI_DEVICE_ID_ADAPTEC_2940:
		ahc->type = AHC_294;
                break;
        case PCI_DEVICE_ID_ADAPTEC_AIC7870:
		ahc->type = AHC_AIC7870;
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

	ahc->sc_ih = pci_map_int(pa->pa_tag, PCI_IPL_BIO, ahcintr, ahc);
}       
