/*	$OpenBSD: bha_pci.c,v 1.10 2009/03/29 21:53:52 sthen Exp $	*/
/*	$NetBSD: bha_pci.c,v 1.16 1998/08/15 10:10:53 mycroft Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/bhareg.h>
#include <dev/ic/bhavar.h>

#define	PCI_CBIO	0x10

int	bha_pci_match(struct device *, void *, void *);
void	bha_pci_attach(struct device *, struct device *, void *);

struct cfattach bha_pci_ca = {
	sizeof(struct bha_softc), bha_pci_match, bha_pci_attach
};

const struct pci_matchid bha_pci_devices[] = {
	{ PCI_VENDOR_BUSLOGIC, PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC },
	{ PCI_VENDOR_BUSLOGIC, PCI_PRODUCT_BUSLOGIC_MULTIMASTER },
};

/*
 * Check the slots looking for a board we recognise
 * If we find one, note its address (slot) and call
 * the actual probe routine to check it out.
 */
int
bha_pci_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t iosize;
	int rv;

	if (pci_matchbyid(pa, bha_pci_devices,
	    sizeof(bha_pci_devices)/sizeof(bha_pci_devices[0])) == 0)
		return (0);

	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0, &iot, &ioh,
	    NULL, &iosize, 0))
		return (0);

	rv = bha_find(iot, ioh, NULL);
	bus_space_unmap(iot, ioh, iosize);

	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
void
bha_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct bha_softc *sc = (void *)self;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t iosize;
	struct bha_probe_data bpd;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *model, *intrstr;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC)
		model = "BusLogic 9xxC SCSI";
	else if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BUSLOGIC_MULTIMASTER)
		model = "BusLogic 9xxC SCSI";
	else
		model = "unknown model!";

	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0, &iot, &ioh,
	    NULL, &iosize, 0)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = pa->pa_dmat;
	if (!bha_find(iot, ioh, &bpd)) {
		printf(": bha_find failed\n");
		bus_space_unmap(iot, ioh, iosize);
		return;
	}

	sc->sc_dmaflags = 0;

	if (pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		bus_space_unmap(iot, ioh, iosize);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, bha_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(iot, ioh, iosize);
		return;
	}
	printf(": %s, %s\n", intrstr, model);

	bha_attach(sc, &bpd);

	bha_disable_isacompat(sc);
}
