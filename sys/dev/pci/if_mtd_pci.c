/*	$OpenBSD: if_mtd_pci.c,v 1.3 2003/08/19 04:52:26 mickey Exp $	*/

/*
 * Copyright (c) 2003 Oleg Safiullin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/bus.h>

#include <dev/mii/miivar.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ic/mtd803reg.h>
#include <dev/ic/mtd803var.h>

static int mtd_pci_match(struct device *, void *, void *);
static void mtd_pci_attach(struct device *, struct device *, void *);

struct cfattach mtd_pci_ca = {
	sizeof(struct mtd_softc), mtd_pci_match, mtd_pci_attach
};

const static struct pci_matchid mtd_pci_devices[] = {
	{ PCI_VENDOR_MYSON, PCI_PRODUCT_MYSON_MTD803 },
};

static int
mtd_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, mtd_pci_devices,
	    sizeof(mtd_pci_devices) / sizeof(mtd_pci_devices[0])));
}

static void
mtd_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct mtd_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t iosize;
	u_int32_t command;

	command = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

#ifndef MTD_USE_IO
	if (pci_mapreg_map(pa, MTD_PCI_LOMEM, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->bus_tag, &sc->bus_handle, NULL, &iosize, 0)) {
		printf(": can't map mem space\n");
		return;
	}
#else	/* !MTD_USE_MEMIO */
	if (pci_mapreg_map(pa, MTD_PCI_LOIO, PCI_MAPREG_TYPE_IO, 0,
	    &sc->bus_tag, &sc->bus_handle, NULL, &iosize, 0)) {
		printf(": can't map io space\n");
		return;
	}
#endif	/* MTD_USE_MEMIO */

	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_space_unmap(sc->bus_tag, sc->bus_handle, iosize);
		return;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	if (pci_intr_establish(pa->pa_pc, ih, IPL_NET, mtd_irq_h, sc,
	    self->dv_xname) == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(sc->bus_tag, sc->bus_handle, iosize);
		return;
	}
	printf(": %s", intrstr);

	sc->dma_tag = pa->pa_dmat;
	mtd_config(sc);
}
