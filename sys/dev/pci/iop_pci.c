/*	$OpenBSD: iop_pci.c,v 1.7 2010/04/08 00:23:53 tedu Exp $	*/
/*	$NetBSD: iop_pci.c,v 1.4 2001/03/20 13:21:00 ad Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

/*
 * PCI front-end for `iop' driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <machine/endian.h>
#include <machine/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/i2o/i2o.h>
#include <dev/i2o/iopreg.h>
#include <dev/i2o/iopio.h>
#include <dev/i2o/iopvar.h>

#define	PCI_INTERFACE_I2O_POLLED	0x00
#define	PCI_INTERFACE_I2O_INTRDRIVEN	0x01

void	iop_pci_attach(struct device *, struct device *, void *);
int	iop_pci_match(struct device *, void *, void *);

struct cfattach iop_pci_ca = {
	sizeof(struct iop_softc), iop_pci_match, iop_pci_attach
};

int
iop_pci_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa;

	pa = aux;

	/* 
	 * Look for an "intelligent I/O processor" that adheres to the I2O
	 * specification.  Ignore the device if it doesn't support interrupt
	 * driven operation.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_I2O &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_I2O_STANDARD &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_I2O_INTRDRIVEN)
		return (1);

	return (0);
}

void
iop_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa;
	struct iop_softc *sc;
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t reg;
	int i;

	sc = (struct iop_softc *)self;
	pa = (struct pci_attach_args *)aux;
	pc = pa->pa_pc;
	printf(": ");

	/*
	 * The kernel always uses the first memory mapping to communicate
	 * with the IOP.
	 */
	for (i = PCI_MAPREG_START; i < PCI_MAPREG_END; i += 4) {
		reg = pci_conf_read(pc, pa->pa_tag, i);
		if (PCI_MAPREG_TYPE(reg) == PCI_MAPREG_TYPE_MEM)
			break;
	}
	if (i == PCI_MAPREG_END) {
		printf("can't find mapping\n");
		return;
	}

	/* Map the register window. */
	if (pci_mapreg_map(pa, i, PCI_MAPREG_TYPE_MEM, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, NULL, 0x40000)) {
		printf("%s: can't map register window\n", sc->sc_dv.dv_xname);
		return;
	}

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_bus_memt = pa->pa_memt;
	sc->sc_bus_iot = pa->pa_iot;

	/* Map and establish the interrupt.  XXX IPL_BIO. */
	if (pci_intr_map(pa, &ih)) {
		printf("can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, iop_intr, sc,
	    sc->sc_dv.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	/* Attach to the bus-independent code. */
	iop_init(sc, intrstr);
}
