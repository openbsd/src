/*	$OpenBSD: if_bcw_pci.c,v 1.14 2007/03/04 11:04:18 mglocker Exp $ */

/*
 * Copyright (c) 2006 Jon Simola <jsimola@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * PCI shim for Broadcom BCM43xx Wireless network chipsets (broadcom.com)
 * SiliconBackplane is technology from Sonics, Inc.(sonicsinc.com)
 */
 
/* standard includes, probably some extras */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
//#include <netinet/in_systm.h>
//#include <netinet/in_var.h>
//#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/bcwreg.h>
#include <dev/ic/bcwvar.h>

#include <uvm/uvm_extern.h>

const struct pci_matchid bcw_pci_devices[]  = {
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4303 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4306 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4306_2 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4307 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4309 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4311 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4312 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4318 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4319 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4322 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM43XG }
};

struct bcw_pci_softc {
	struct bcw_softc	 psc_bcw;		/* Real softc */
	pci_intr_handle_t	 psc_ih;		/* interrupt handle */
	void			*psc_intrcookie;
	pci_chipset_tag_t	 psc_pc;		/* our PCI chipset */
	pcitag_t		 psc_pcitag;		/* our PCI tag */
};

int		bcw_pci_match(struct device *, void *, void *);
void		bcw_pci_attach(struct device *, struct device *, void *);
void		bcw_pci_conf_write(void *, uint32_t, uint32_t);
uint32_t	bcw_pci_conf_read(void *, uint32_t);

struct cfattach bcw_pci_ca = {
	sizeof(struct bcw_pci_softc), bcw_pci_match, bcw_pci_attach
};

int
bcw_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, bcw_pci_devices,
	    sizeof(bcw_pci_devices) / sizeof(bcw_pci_devices[0])));
}

void
bcw_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct bcw_pci_softc *psc = (void *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	struct bcw_softc *sc = &psc->psc_bcw;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcireg_t memtype;
	bus_addr_t memaddr;
	bus_size_t memsize;
	int pmreg;
	pcireg_t pmode;

	sc->sc_dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;
	psc->psc_pcitag = pa->pa_tag;
	sc->sc_dev_softc = psc;

	/* Get it out of power save mode if needed. */
	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PWRMGMT, &pmreg, 0)) {
		pmode = pci_conf_read(pc, pa->pa_tag, pmreg + 4) & 0x3;
		if (pmode == 3) {
			/*
			 * The card has lost all configuration data in
			 * this state, so punt.
			 */
			printf("%s: unable to wake up from power state D3\n",
			    sc->sc_dev.dv_xname);
			return;
		}
		if (pmode != 0) {
			printf("%s: waking up from power state D%d\n",
			    sc->sc_dev.dv_xname, pmode);
			pci_conf_write(pc, pa->pa_tag, pmreg + 4, 0);
		}
	}

	/*
	 * Map control/status registers.
	 */
	/* Copied from pre-abstraction, via if_bce.c */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, BCW_PCI_BAR0);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		if (pci_mapreg_map(pa, BCW_PCI_BAR0, memtype, 0, &sc->sc_iot,
		    &sc->sc_ioh, &memaddr, &memsize, 0) == 0)
			break;
	default:
		printf("%s: unable to find mem space\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* Map the PCI interrupt */
	if (pci_intr_map(pa, &psc->psc_ih)) {
		printf("%s: couldn't map interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	sc->bcw_intrstr = pci_intr_string(pc, psc->psc_ih);

	psc->psc_intrcookie = pci_intr_establish(pc, psc->psc_ih, IPL_NET, 
	    bcw_intr, sc, sc->sc_dev.dv_xname);

	if (psc->psc_intrcookie == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (sc->bcw_intrstr != NULL)
			printf(" at %s", sc->bcw_intrstr);
		printf("\n");
		return;
	}

	printf(": %s", sc->bcw_intrstr);

	sc->sc_conf_write = bcw_pci_conf_write;
	sc->sc_conf_read = bcw_pci_conf_read;

	/*
	 * Get some PCI based info into the softc
	 */
	sc->sc_board_vendor = PCI_VENDOR(pa->pa_id);
	sc->sc_prodid = PCI_PRODUCT(pa->pa_id); /* XXX */
	sc->sc_board_type = sc->sc_prodid; /* XXX */
	sc->sc_board_rev = PCI_REVISION(pa->pa_class);

	/*
	 * Start the card up while we're in PCI land
	 */

	/* Turn the Crystal On */
	bcw_powercontrol_crystal_on(sc);

	/*
	 * Clear PCI_STATUS_TARGET_TARGET_ABORT, Docs and Linux call it 
	 * PCI_STATUS_SIG_TARGET_ABORT - should use pci_conf_read/write?
	 */
	pci_conf_write(pa->pa_pc, pa->pa_tag,
	    PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pa->pa_pc, pa->pa_tag,
	    PCI_COMMAND_STATUS_REG)
	    & ~PCI_STATUS_TARGET_TARGET_ABORT);

	/*
	 * Finish the attach
	 */
	bcw_attach(sc);
}

void
bcw_pci_conf_write(void *self, uint32_t reg, uint32_t val)
{
	struct bcw_pci_softc *psc = (struct bcw_pci_softc *)self;

	pci_conf_write(psc->psc_pc, psc->psc_pcitag, reg, val);
}

uint32_t
bcw_pci_conf_read(void *self, uint32_t reg)
{
	struct bcw_pci_softc *psc = (struct bcw_pci_softc *)self;

	return (pci_conf_read(psc->psc_pc, psc->psc_pcitag, reg));
}
