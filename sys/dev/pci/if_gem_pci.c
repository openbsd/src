/*	$OpenBSD: if_gem_pci.c,v 1.18 2005/09/30 18:59:13 kettenis Exp $	*/
/*	$NetBSD: if_gem_pci.c,v 1.1 2001/09/16 00:11:42 eeh Exp $ */

/*
 *
 * Copyright (C) 2001 Eduardo Horvath.
 * All rights reserved.
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * PCI bindings for Sun GEM ethernet controllers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#ifdef __sparc64__
#include <dev/ofw/openfirm.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/gemreg.h>
#include <dev/ic/gemvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

struct gem_pci_softc {
	struct	gem_softc	gsc_gem;	/* GEM device */
	bus_space_tag_t		gsc_memt;
	bus_space_handle_t	gsc_memh;
	void			*gsc_ih;
};

int	gem_match_pci(struct device *, void *, void *);
void	gem_attach_pci(struct device *, struct device *, void *);

struct cfattach gem_pci_ca = {
	sizeof(struct gem_pci_softc), gem_match_pci, gem_attach_pci
};

/*
 * Attach routines need to be split out to different bus-specific files.
 */

const struct pci_matchid gem_pci_devices[] = {
	{ PCI_VENDOR_SUN, PCI_PRODUCT_SUN_ERINETWORK },
	{ PCI_VENDOR_SUN, PCI_PRODUCT_SUN_GEMNETWORK },
	{ PCI_VENDOR_APPLE, PCI_PRODUCT_APPLE_GMAC },
	{ PCI_VENDOR_APPLE, PCI_PRODUCT_APPLE_GMAC2 },
	{ PCI_VENDOR_APPLE, PCI_PRODUCT_APPLE_GMAC3 },
	{ PCI_VENDOR_APPLE, PCI_PRODUCT_APPLE_K2_GMAC },
	{ PCI_VENDOR_APPLE, PCI_PRODUCT_APPLE_SHASTA_GMAC }
};

int
gem_match_pci(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	return (pci_matchbyid((struct pci_attach_args *)aux, gem_pci_devices,
	    sizeof(gem_pci_devices)/sizeof(gem_pci_devices[0])));
}

void
gem_attach_pci(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct gem_pci_softc *gsc = (void *)self;
	struct gem_softc *sc = &gsc->gsc_gem;
	pci_intr_handle_t intrhandle;
#ifdef __sparc64__
	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr(u_char *);
#endif
	const char *intrstr;
	int type;

	if (pa->pa_memt) {
		type = PCI_MAPREG_TYPE_MEM;
		sc->sc_bustag = pa->pa_memt;
	} else {
		type = PCI_MAPREG_TYPE_IO;
		sc->sc_bustag = pa->pa_iot;
	}

	sc->sc_dmatag = pa->pa_dmat;

	sc->sc_pci = 1; /* XXXXX should all be done in bus_dma. */

#define PCI_GEM_BASEADDR	0x10
	if (pci_mapreg_map(pa, PCI_GEM_BASEADDR, type, 0,
	    &gsc->gsc_memt, &gsc->gsc_memh, NULL, NULL, 0) != 0)
	{
		printf(": could not map gem registers\n");
		return;
	}

	sc->sc_bustag = gsc->gsc_memt;
	sc->sc_h = gsc->gsc_memh;

#ifdef __sparc64__
	if (OF_getprop(PCITAG_NODE(pa->pa_tag), "local-mac-address",
	    sc->sc_enaddr, ETHER_ADDR_LEN) <= 0)
		myetheraddr(sc->sc_enaddr);
#endif
#ifdef __powerpc__ 
        pci_ether_hw_addr(pa->pa_pc, sc->sc_enaddr);
#endif

	sc->sc_burst = 16;	/* XXX */

	if (pci_intr_map(pa, &intrhandle) != 0) {
		printf(": couldn't map interrupt\n");
		return;	/* bus_unmap ? */
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle);
	gsc->gsc_ih = pci_intr_establish(pa->pa_pc,
	    intrhandle, IPL_NET, gem_intr, sc, self->dv_xname);
	if (gsc->gsc_ih != NULL) {
		printf(": %s", intrstr ? intrstr : "unknown interrupt");
	} else {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;	/* bus_unmap ? */
	}

	/*
	 * call the main configure
	 */
	gem_config(sc);
}
