/*	$OpenBSD: if_le_pci.c,v 1.25 2005/12/17 07:31:27 miod Exp $	*/
/*	$NetBSD: if_le_pci.c,v 1.13 1996/10/25 21:33:32 cgd Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <dev/pci/if_levar.h>

#ifdef __alpha__			/* XXX */
/* XXX XXX NEED REAL DMA MAPPING SUPPORT XXX XXX */ 
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap((vaddr_t)(va))
#endif

int le_pci_match(struct device *, void *, void *);
void le_pci_attach(struct device *, struct device *, void *);

struct cfattach le_pci_ca = {
	sizeof(struct le_softc), le_pci_match, le_pci_attach
};

hide void le_pci_wrcsr(struct am7990_softc *, u_int16_t, u_int16_t);
hide u_int16_t le_pci_rdcsr(struct am7990_softc *, u_int16_t);

/*
 * PCI constants.
 * XXX These should be in a common file!
 */
#define PCI_CBIO	0x10		/* Configuration Base IO Address */

hide void
le_pci_wrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	bus_space_write_2(iot, ioh, lesc->sc_rdp, val);
}

hide u_int16_t
le_pci_rdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;
	u_int16_t val;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	val = bus_space_read_2(iot, ioh, lesc->sc_rdp);
	return (val);
}

int
le_pci_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_AMD)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_AMD_PCNET_PCI:
	case PCI_PRODUCT_AMD_PCHOME_PCI:
		return (1);
	}

	return (0);
}

void
le_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct le_softc *lesc = (void *)self;
	struct am7990_softc *sc = &lesc->sc_am7990;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	bus_size_t iosize;
	bus_space_handle_t ioh;
	bus_space_tag_t iot;
	pci_chipset_tag_t pc = pa->pa_pc;
	int i, rseg;
	const char *intrstr;
	bus_dma_segment_t seg;
	bus_dmamap_t dmamap;
	caddr_t kva;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_AMD_PCNET_PCI:
		lesc->sc_rap = PCNET_PCI_RAP;
		lesc->sc_rdp = PCNET_PCI_RDP;
		break;
	}

	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, &iosize, 0)) {
		printf(": can't map I/O base\n");
		return;
	}

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++)
		sc->sc_arpcom.ac_enaddr[i] = bus_space_read_1(iot, ioh, i);

	if (bus_dmamem_alloc(pa->pa_dmat, PCNET_MEMSIZE, PAGE_SIZE,
	    0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf(": couldn't allocate memory for card\n");
		bus_space_unmap(iot, ioh, iosize);
		return;
	}
	if (bus_dmamem_map(pa->pa_dmat, &seg, rseg, PCNET_MEMSIZE,
	    &kva, BUS_DMA_NOWAIT)) {
		printf(": couldn't map memory for card\n");
		bus_dmamem_free(pa->pa_dmat, &seg, rseg);
		bus_space_unmap(iot, ioh, iosize);
		return;
	}
	if (bus_dmamap_create(pa->pa_dmat, PCNET_MEMSIZE, 1, PCNET_MEMSIZE,
	    0, BUS_DMA_NOWAIT, &dmamap)) {
		printf(": couldn't create dma map\n");
		bus_dmamem_unmap(pa->pa_dmat, kva, PCNET_MEMSIZE);
		bus_dmamem_free(pa->pa_dmat, &seg, rseg);
		bus_space_unmap(iot, ioh, iosize);
		return;
	}
	if (bus_dmamap_load(pa->pa_dmat, dmamap, kva, PCNET_MEMSIZE,
	    NULL, BUS_DMA_NOWAIT)) {
		printf(": couldn't load dma map\n");
		bus_dmamap_destroy(pa->pa_dmat, dmamap);
		bus_dmamem_unmap(pa->pa_dmat, kva, PCNET_MEMSIZE);
		bus_dmamem_free(pa->pa_dmat, &seg, rseg);
		bus_space_unmap(iot, ioh, iosize);
		return;
	}
	sc->sc_mem = kva;
	bzero(sc->sc_mem, PCNET_MEMSIZE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_dmamap_destroy(pa->pa_dmat, dmamap);
		bus_dmamem_unmap(pa->pa_dmat, kva, PCNET_MEMSIZE);
		bus_dmamem_free(pa->pa_dmat, &seg, rseg);
		bus_space_unmap(iot, ioh, iosize);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	lesc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, am7990_intr, sc,
	    sc->sc_dev.dv_xname);
	if (lesc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		bus_dmamap_destroy(pa->pa_dmat, dmamap);
		bus_dmamem_unmap(pa->pa_dmat, kva, PCNET_MEMSIZE);
		bus_dmamem_free(pa->pa_dmat, &seg, rseg);
		bus_space_unmap(iot, ioh, iosize);
		return;
	}
	printf(": %s\n", intrstr);

	lesc->sc_iot = iot;
	lesc->sc_ioh = ioh;

	sc->sc_conf3 = 0;
	sc->sc_addr = vtophys((vaddr_t)sc->sc_mem);	/* XXX XXX XXX */
	sc->sc_memsize = PCNET_MEMSIZE;

	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;

	sc->sc_rdcsr = le_pci_rdcsr;
	sc->sc_wrcsr = le_pci_wrcsr;
	sc->sc_hwreset = NULL;
	sc->sc_hwinit = NULL;

	printf("%s", sc->sc_dev.dv_xname);
	am7990_config(sc);
}
