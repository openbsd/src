/*	$OpenBSD: if_fxp_pci.c,v 1.16 2002/11/19 18:40:17 jason Exp $	*/

/*
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Modifications to support NetBSD:
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
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
 *	Id: if_fxp.c,v 1.55 1998/08/04 08:53:12 dg Exp
 */

/*
 * Intel EtherExpress Pro/100B PCI Fast Ethernet driver
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <netinet/if_ether.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/mii/miivar.h>

#include <dev/ic/fxpreg.h>
#include <dev/ic/fxpvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

int fxp_pci_match(struct device *, void *, void *);
void fxp_pci_attach(struct device *, struct device *, void *);

struct cfattach fxp_pci_ca = {
	sizeof(struct fxp_softc), fxp_pci_match, fxp_pci_attach
};

const struct pci_matchid fxp_pci_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82557 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82559 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82559ER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82562 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_100_VE_0 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_100_VE_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_100_VE_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_100_VM_0 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_100_VM_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_100_VM_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_100_VM_3 },
};

/*
 * Check if a device is an 82557.
 */
int
fxp_pci_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (pci_matchbyid((struct pci_attach_args *)aux, fxp_pci_devices,
	    sizeof(fxp_pci_devices)/sizeof(fxp_pci_devices[0])));
}

void
fxp_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct fxp_softc *sc = (struct fxp_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	u_int8_t enaddr[6];
	bus_space_tag_t iot = pa->pa_iot;
	bus_addr_t iobase;
	bus_size_t iosize;
	pcireg_t rev = PCI_REVISION(pa->pa_class);

	if (pci_io_find(pc, pa->pa_tag, FXP_PCI_IOBA, &iobase, &iosize)) {
		printf(": can't find i/o space\n");
		return;
	}

	if (bus_space_map(iot, iobase, iosize, 0, &sc->sc_sh)) {
		printf(": can't map i/o space\n");
		return;
	}
	sc->sc_st = iot;
	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, fxp_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_82562:
		sc->sc_flags |= FXPF_HAS_RESUME_BUG;
		/* FALLTHROUGH */
	case PCI_PRODUCT_INTEL_82559:
	case PCI_PRODUCT_INTEL_82559ER:
		sc->not_82557 = 1;
		break;
	case PCI_PRODUCT_INTEL_82557:
		/*
		 * revisions
		 * 2 = 82557
		 * 4-6 = 82558
		 * 8 = 82559
		 */
		sc->not_82557 = (rev >= 4) ? 1 : 0;
		break;
	case PCI_PRODUCT_INTEL_PRO_100_VE_0:
	case PCI_PRODUCT_INTEL_PRO_100_VE_1:
	case PCI_PRODUCT_INTEL_PRO_100_VE_2:
	case PCI_PRODUCT_INTEL_PRO_100_VM_0:
	case PCI_PRODUCT_INTEL_PRO_100_VM_1:
	case PCI_PRODUCT_INTEL_PRO_100_VM_2:
	case PCI_PRODUCT_INTEL_PRO_100_VM_3:
		sc->sc_flags |= FXPF_HAS_RESUME_BUG;
		sc->not_82557 = 0;
		break;
	default:
		sc->not_82557 = 0;
		break;
	}

	/* Do generic parts of attach. */
	if (fxp_attach_common(sc, enaddr, intrstr)) {
		/* Failed! */
		return;
	}
}
