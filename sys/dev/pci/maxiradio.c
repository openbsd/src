/*	$OpenBSD: maxiradio.c,v 1.1 2001/10/04 20:25:28 gluk Exp $	*/
/* $RuOBSD: maxiradio.c,v 1.3 2001/10/02 10:45:53 pva Exp $ */

/*
 * Copyright (c) 2001 Maxim Tsyplakov <tm@oganer.net>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Guillemot Maxi Radio FM2000 PCI Radio Card Device Driver */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/radio_if.h>

int	mr_match(struct device *, void *, void *);
void	mr_attach(struct device *, struct device *, void *);
int	mr_open(dev_t, int, int, struct proc *);
int	mr_close(dev_t, int, int, struct proc *);
int	mr_ioctl(dev_t, u_long, caddr_t, int, struct proc *);

/* config base I/O address ? */
#define PCI_CBIO 0x6400	

/* define our interface to the high-level radio driver */

struct radio_hw_if mr_hw_if = {
	mr_open,
	mr_close,
	mr_ioctl
};

struct mr_softc {
	struct device	sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

struct cfattach mr_ca = {
	sizeof(struct mr_softc), mr_match, mr_attach
};

struct cfdriver mr_cd = {
	NULL, "mr", DV_DULL
};

int
mr_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_GUILLEMOT &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_GUILLEMOT_MAXIRADIO)
		return (1);
	return (0);
}

void
mr_attach(struct device *parent, struct device *self, void *aux)
{
	struct mr_softc *sc = (struct mr_softc *) self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	bus_addr_t iobase;
	bus_size_t iosize;
	pcireg_t csr;
	
	if (pci_io_find(pc, pa->pa_tag, PCI_CBIO, &iobase, &iosize)) {
		printf (": can't find i/o base\n");
		return;
	}

	if (bus_space_map(sc->sc_iot = pa->pa_iot, iobase, iosize,
			  0, &sc->sc_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}	
	
	/* Enable the card */
	csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);
	
	radio_attach_mi(&mr_hw_if, sc, &sc->sc_dev);
}

int
mr_open(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct mr_softc *sc;
	return !(sc = mr_cd.cd_devs[0]) ? ENXIO : 0;
}

int
mr_close(dev_t dev, int flags, int fmt, struct proc *p)
{
	return 0;
}

/*
 * Handle the ioctl for the device
 */

int
mr_ioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	int	error;

	error = 0;
	switch (cmd) {
	default:
		error = EINVAL;	/* invalid agument */
	}
	return error;
}
