/*	$OpenBSD: if_wi_pci.c,v 1.11 2001/12/20 17:41:48 mickey Exp $	*/

/*
 * Copyright (c) 2001 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is a PCI shim for the Wavelan wireless network driver.
 * It works with PCI adaptors based on the PLX 9050 and PLX 9052
 * PCI to "dumb bus" bridge chip.  It has been tested with the
 * Global Sun Technology GL24110P02 (aka Linksys WDT11), 3Com 3CRWE777A,
 * and Netgear MA301.  It is also expected to work with the
 * Global Sun GL24110P and Eumitcom WL11000P.
 *
 * All we do here is handle the PCI match and attach, set up an
 * interrupt handler entry point, and setup the PLX chip for level
 * interrupts and config index 1.
 *
 * The PLX 9052 provides us with multiple PCI address space mappings.
 * The primary mappings at PCI registers 0x10 (mem) and 0x14 (I/O) are for
 * the PLX chip itself, *NOT* the pcmcia card.
 * The PLX 9052 provides 4 local address space registers: 0x18, 0x1C,
 * 0x20, and 0x24.  The mem and I/O spaces for the PCMCIA card are
 * mapped to 0x18 and 0x1C respectively.
 *
 * The datasheet may be downloaded from PLX (though you do have to register)
 * http://www.plxtech.com/products/toolbox/9050.htm
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_ieee80211.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/if_wireg.h>
#include <dev/ic/if_wi_ieee.h>
#include <dev/ic/if_wivar.h>

#define WI_PCI_CBMA		0x10
#define WI_PCI_PLX_LOMEM	0x10	/* PLX chip membase */
#define WI_PCI_PLX_LOIO		0x14	/* PLX chip iobase */
#define WI_PCI_LOMEM		0x18	/* ISA membase */
#define WI_PCI_LOIO		0x1C	/* ISA iobase */

const struct wi_pci_product *wi_pci_lookup __P((struct pci_attach_args *pa));
int	wi_pci_match	__P((struct device *, void *, void *));
void	wi_pci_attach	__P((struct device *, struct device *, void *));
int	wi_intr		__P((void *));
int	wi_attach	__P((struct wi_softc *, int));

struct cfattach wi_pci_ca = {
	sizeof (struct wi_softc), wi_pci_match, wi_pci_attach
};

static const struct wi_pci_product {
	pci_vendor_id_t pp_vendor;
	pci_product_id_t pp_product;
	int pp_plx;
} wi_pci_products[] = {
	{ PCI_VENDOR_GLOBALSUN, PCI_PRODUCT_GLOBALSUN_GL24110P, 1 },
	{ PCI_VENDOR_GLOBALSUN, PCI_PRODUCT_GLOBALSUN_GL24110P02, 1 },
	{ PCI_VENDOR_EUMITCOM, PCI_PRODUCT_EUMITCOM_WL11000P, 1 },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CRWE777A, 1 },
	{ PCI_VENDOR_NETGEAR, PCI_PRODUCT_NETGEAR_MA301, 1 },
	{ PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_MINI_PCI_WLAN, 0 },
	{ 0, 0 }
};

const struct wi_pci_product *
wi_pci_lookup(pa)
	struct pci_attach_args *pa;
{
	const struct wi_pci_product *pp;

	for (pp = wi_pci_products; pp->pp_product != 0; pp++) {
		if (PCI_VENDOR(pa->pa_id) == pp->pp_vendor && 
		    PCI_PRODUCT(pa->pa_id) == pp->pp_product)
			return (pp);
	}

	return (NULL);
}

int
wi_pci_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (wi_pci_lookup(aux) != NULL);
}

void
wi_pci_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct wi_softc *sc = (struct wi_softc *)self;
	struct pci_attach_args *pa = aux;
	const struct wi_pci_product *pp;
	pci_intr_handle_t ih;
	bus_space_handle_t ioh, memh;
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_tag_t memt = pa->pa_memt;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcireg_t csr;
	const char *intrstr;

	pp = wi_pci_lookup(pa);
	if (pp->pp_plx) {
		/* Map memory and I/O registers. */
		if (pci_mapreg_map(pa, WI_PCI_LOMEM, PCI_MAPREG_TYPE_MEM, 0,
		    &memt, &memh, NULL, NULL, 0) != 0) {
			printf(": can't map mem space\n");
			return;
		}
		if (pci_mapreg_map(pa, WI_PCI_LOIO, PCI_MAPREG_TYPE_IO, 0,
		    &iot, &ioh, NULL, NULL, 0) != 0) {
			printf(": can't map I/O space\n");
			return;
		}
	} else {
		if (pci_mapreg_map(pa, WI_PCI_CBMA, PCI_MAPREG_TYPE_MEM,
		    0, &iot, &ioh, NULL, NULL, 0) != 0) {
			printf(": can't map mem space\n");
			return;
		}

		memt = iot;
		memh = ioh;
	}

	sc->wi_btag = iot;
	sc->wi_bhandle = ioh;

	/* Enable the card. */
	csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	/* Make sure interrupts are disabled. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, wi_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s", intrstr);

	if (pp->pp_plx) {
		/*
		 * Setup the PLX chip for level interrupts and config index 1
		 * XXX - should really reset the PLX chip too.
		 */
		bus_space_write_1(memt, memh,
		    WI_PLX_COR_OFFSET, WI_PLX_COR_VALUE);

		wi_attach(sc, 1);
	} else {
		bus_space_write_2(iot, ioh, WI_PCI_COR, WI_PCI_SOFT_RESET);
		DELAY(100*1000); /* 100 m sec */
		bus_space_write_2(iot, ioh, WI_PCI_COR, 0x0);
		DELAY(100*1000); /* 100 m sec */

		wi_attach(sc, 0);
	}
}
