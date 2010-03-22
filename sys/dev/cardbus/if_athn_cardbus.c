/*	$OpenBSD: if_athn_cardbus.c,v 1.6 2010/03/22 22:28:27 jsg Exp $	*/

/*-
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
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
 * CardBus front-end for Atheros 802.11a/g/n chipsets.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/athnreg.h>
#include <dev/ic/athnvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>

struct athn_cardbus_softc {
	struct athn_softc	sc_sc;

	/* CardBus specific goo. */
	cardbus_devfunc_t	sc_ct;
	pcitag_t		sc_tag;
	void			*sc_ih;
	bus_size_t		sc_mapsize;
	pcireg_t		sc_bar_val;
	int			sc_intrline;
};

int	athn_cardbus_match(struct device *, void *, void *);
void	athn_cardbus_attach(struct device *, struct device *, void *);
int	athn_cardbus_detach(struct device *, int);
int	athn_cardbus_enable(struct athn_softc *);
void	athn_cardbus_disable(struct athn_softc *);
void	athn_cardbus_power(struct athn_softc *, int);
void	athn_cardbus_setup(struct athn_cardbus_softc *);

struct cfattach athn_cardbus_ca = {
	sizeof (struct athn_cardbus_softc),
	athn_cardbus_match,
	athn_cardbus_attach,
	athn_cardbus_detach
};

static const struct pci_matchid athn_cardbus_devices[] = {
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5416 },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5418 },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9160 },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9280 },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9281 },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9285 },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR2427 },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9227 },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9287 }
};

int
athn_cardbus_match(struct device *parent, void *match, void *aux)
{
	return (cardbus_matchbyid(aux, athn_cardbus_devices,
	    nitems(athn_cardbus_devices)));
}

void
athn_cardbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)self;
	struct athn_softc *sc = &csc->sc_sc;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	bus_addr_t base;
	int error;

	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;
	csc->sc_intrline = ca->ca_intrline;

	/* Power management hooks. */
	sc->sc_enable = athn_cardbus_enable;
	sc->sc_disable = athn_cardbus_disable;
	sc->sc_power = athn_cardbus_power;

	/* Map control/status registers. */
	error = Cardbus_mapreg_map(ct, CARDBUS_BASE0_REG,
	    PCI_MAPREG_TYPE_MEM, 0, &sc->sc_st, &sc->sc_sh, &base,
	    &csc->sc_mapsize);
	if (error != 0) {
		printf(": can't map mem space\n");
		return;
	}
	csc->sc_bar_val = base | PCI_MAPREG_TYPE_MEM;

	/* Set up the PCI configuration registers. */
	athn_cardbus_setup(csc);

	printf(": irq %d", csc->sc_intrline);

	athn_attach(sc);
	Cardbus_function_disable(ct);
}

int
athn_cardbus_detach(struct device *self, int flags)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)self;
	struct athn_softc *sc = &csc->sc_sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	pci_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	athn_detach(sc);

	/* Unhook the interrupt handler. */
	if (csc->sc_ih != NULL)
		cardbus_intr_disestablish(cc, cf, csc->sc_ih);

	/* Release bus space and close window. */
	Cardbus_mapreg_unmap(ct, CARDBUS_BASE0_REG, sc->sc_st, sc->sc_sh,
	    csc->sc_mapsize);

	return (0);
}

int
athn_cardbus_enable(struct athn_softc *sc)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	pci_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* Power on the socket. */
	Cardbus_function_enable(ct);

	/* Setup the PCI configuration registers. */
	athn_cardbus_setup(csc);

	/* Map and establish the interrupt handler. */
	csc->sc_ih = cardbus_intr_establish(cc, cf, csc->sc_intrline, IPL_NET,
	    athn_intr, sc, sc->sc_dev.dv_xname);
	if (csc->sc_ih == NULL) {
		printf("%s: could not establish interrupt at %d\n",
		    sc->sc_dev.dv_xname, csc->sc_intrline);
		Cardbus_function_disable(ct);
		return (1);
	}
	return (0);
}

void
athn_cardbus_disable(struct athn_softc *sc)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	pci_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* Unhook the interrupt handler. */
	cardbus_intr_disestablish(cc, cf, csc->sc_ih);
	csc->sc_ih = NULL;

	/* Power down the socket. */
	Cardbus_function_disable(ct);
}

void
athn_cardbus_power(struct athn_softc *sc, int why)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)sc;

	if (why == PWR_RESUME) {
		/* Restore the PCI configuration registers. */
		athn_cardbus_setup(csc);
	}
}

void
athn_cardbus_setup(struct athn_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->sc_ct;
	pci_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcireg_t reg;

	/* Program the BAR. */
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_BASE0_REG,
	    csc->sc_bar_val);

	/* Make sure the right access type is on the cardbus bridge. */
	(*cf->cardbus_ctrl)(cc, CARDBUS_MEM_ENABLE);
	(*cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);

	/* Enable the appropriate bits in the PCI CSR. */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag,
	    PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE;
	cardbus_conf_write(cc, cf, csc->sc_tag, PCI_COMMAND_STATUS_REG,
	    reg);

	/*
	 * Noone knows why this shit is necessary but there are claims that
	 * not doing this may cause very frequent PCI FATAL interrupts from
	 * the card: http://bugzilla.kernel.org/show_bug.cgi?id=13483
	 */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag, 0x40);
	reg &= ~0xff00;
	cardbus_conf_write(cc, cf, csc->sc_tag, 0x40, reg);

	/* Change latency timer; default value yields poor results. */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag, PCI_BHLC_REG);
	reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
	reg |= 168 << PCI_LATTIMER_SHIFT;
	cardbus_conf_write(cc, cf, csc->sc_tag, PCI_BHLC_REG, reg);
}
