/*	$NetBSD: if_ep_pci.c,v 1.7 1996/05/13 00:03:15 mycroft Exp $	*/

/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@beer.org>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bpfilter.h" 
 
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h> 
#include <sys/socket.h> 
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h> 
#include <netinet/if_ether.h>
#endif
 
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.old.h>
#include <machine/intr.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

/*
 * PCI constants.
 * XXX These should be in a common file!
 */
#define PCI_CONN		0x48    /* Connector type */
#define PCI_CBIO		0x10    /* Configuration Base IO Address */

int ep_pci_match __P((struct device *, void *, void *));
void ep_pci_attach __P((struct device *, struct device *, void *));

struct cfattach ep_pci_ca = {
	sizeof(struct ep_softc), ep_pci_match, ep_pci_attach
};

int
ep_pci_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_3COM)
		return 0;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_3COM_3C590:
	case PCI_PRODUCT_3COM_3C595:
	case PCI_PRODUCT_3COM_3C595T:
	case PCI_PRODUCT_3COM_3C595TM:
	case PCI_PRODUCT_3COM_3C900:
	case PCI_PRODUCT_3COM_3C900T:
	case PCI_PRODUCT_3COM_3C905:
	case PCI_PRODUCT_3COM_3C905T:
		break;
	default:
		return 0;
	}

	return 1;
}

void
ep_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ep_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	bus_chipset_tag_t bc = pa->pa_bc;
	bus_io_addr_t iobase;
	bus_io_size_t iosize;
	pci_intr_handle_t ih;
	u_short conn = 0;
	pcireg_t i;
	char *model;
	const char *intrstr = NULL;

	if (pci_io_find(pc, pa->pa_tag, PCI_CBIO, &iobase, &iosize)) {
		printf(": can't find i/o space\n");
		return;
	}

	if (bus_io_map(bc, iobase, iosize, &sc->sc_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_bc = bc;
	sc->bustype = EP_BUS_PCI;

	i = pci_conf_read(pc, pa->pa_tag, PCI_CONN);

	/*
	 * Bits 13,12,9 of the isa adapter are the same as bits 
	 * 5,4,3 of the pci adapter
	 */
	if (i & IS_PCI_AUI)
		conn |= IS_AUI;
	if (i & IS_PCI_BNC)
		conn |= IS_BNC;
	if (i & IS_PCI_UTP)
		conn |= IS_UTP;

	GO_WINDOW(0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_3COM_3C590:
		model = "3Com 3C590 Ethernet";
		break;
	case PCI_PRODUCT_3COM_3C595:
	case PCI_PRODUCT_3COM_3C595T:
	case PCI_PRODUCT_3COM_3C595TM:
		model = "3Com 3C595 Ethernet";
		break;
	case PCI_PRODUCT_3COM_3C900:
	case PCI_PRODUCT_3COM_3C900T:
		model = "3Com 3C900 Ethernet";
		break;
	case PCI_PRODUCT_3COM_3C905:
	case PCI_PRODUCT_3COM_3C905T:
		model = "3Com 3C905 Ethernet";
		break;
	default:
		model = "unknown model!";
	}

	printf(": <%s> ", model);

	epconfig(sc, conn);

	/* Enable the card. */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, epintr,
	    sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	epstop(sc);
}
