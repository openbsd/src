/*	$OpenBSD: if_bcw_cardbus.c,v 1.4 2006/12/06 19:21:45 mglocker Exp $ */

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

/* CardBus attachment for Broadcom BCM43xx 802.11 wireless */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>

#include <dev/ic/bcwreg.h>
#include <dev/ic/bcwvar.h>

struct bcw_cardbus_softc {
	struct bcw_softc	sc_bcw;

	/* cardbus specific goo */
	cardbus_devfunc_t	sc_ct;
	cardbustag_t		sc_tag;
	void			*sc_ih;

	bus_size_t		sc_mapsize;
	pcireg_t		sc_bar_val;
	int			sc_intrline;
};

int	bcw_cardbus_match(struct device *, void *, void *);
void	bcw_cardbus_attach(struct device *, struct device *, void *);
int	bcw_cardbus_detach(struct device *, int);
void	bcw_cardbus_power(struct bcw_softc *, int);
void	bcw_cardbus_setup(struct bcw_cardbus_softc *);
int	bcw_cardbus_enable(struct bcw_softc *);
void	bcw_cardbus_disable(struct bcw_softc *);
void	bcw_cardbus_conf_write(struct bcw_softc *, u_int32_t, u_int32_t);
u_int32_t	bcw_cardbus_conf_read(struct bcw_softc *, u_int32_t);

struct cfattach bcw_cardbus_ca = {
	sizeof (struct bcw_cardbus_softc), bcw_cardbus_match,
	bcw_cardbus_attach, bcw_cardbus_detach
};

static const struct cardbus_matchid bcw_cardbus_devices[] = {
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

int
bcw_cardbus_match(struct device *parent, void *match, void *aux)
{
	return (cardbus_matchbyid(aux, bcw_cardbus_devices,
	    sizeof (bcw_cardbus_devices) / sizeof (bcw_cardbus_devices[0])));
}

void
bcw_cardbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct bcw_cardbus_softc *csc = (struct bcw_cardbus_softc *)self;
	struct cardbus_attach_args *ca = aux;
	struct bcw_softc *sc = &csc->sc_bcw;
	cardbus_devfunc_t ct = ca->ca_ct;
	bus_addr_t base;
	int error;

	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;
	csc->sc_intrline = ca->ca_intrline;
	sc->sc_ca.ca_tag = ca->ca_tag;
	sc->sc_ca.ca_ct = ca->ca_ct;

	/* power management hooks */
	sc->sc_enable = bcw_cardbus_enable;
	sc->sc_disable = bcw_cardbus_disable;
	sc->sc_power = bcw_cardbus_power;

	/* config register read/write functions */
	sc->sc_conf_read = bcw_cardbus_conf_read;
	sc->sc_conf_write = bcw_cardbus_conf_write;

	/* map control/status registers */
	error = Cardbus_mapreg_map(ct, CARDBUS_BASE0_REG,
	    CARDBUS_MAPREG_TYPE_MEM, 0, &sc->sc_iot,
	    &sc->sc_ioh, &base, &csc->sc_mapsize);
	if (error != 0) {
		printf(": could not map 1st memory space\n");
		return;
	}
	csc->sc_bar_val = base | CARDBUS_MAPREG_TYPE_MEM;

	/* set up the PCI configuration registers */
	bcw_cardbus_setup(csc);

	printf(": irq %d", csc->sc_intrline);

	/*
	 * Get some cardbus info into the softc
	 */
	sc->sc_chiprev=PCI_REVISION(ca->ca_class);
	sc->sc_prodid=CARDBUS_PRODUCT(ca->ca_id);

#if 0
	error = bcw_attach(sc);
	if (error != 0)
		bcw_cardbus_detach(&sc->sc_dev, 0);
#endif
	bcw_attach(sc);

	Cardbus_function_disable(ct);
}

int
bcw_cardbus_detach(struct device *self, int flags)
{
	struct bcw_cardbus_softc *csc = (struct bcw_cardbus_softc *)self;
	struct bcw_softc *sc = &csc->sc_bcw;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	int error;

	error = bcw_detach(sc);
	if (error != 0)
		return (error);

	/* unhook the interrupt handler */
	if (csc->sc_ih != NULL) {
		cardbus_intr_disestablish(cc, cf, csc->sc_ih);
		csc->sc_ih = NULL;
	}

	/* release bus space and close window */
	Cardbus_mapreg_unmap(ct, CARDBUS_BASE0_REG, sc->sc_iot,
	    sc->sc_ioh, csc->sc_mapsize);

	return (0);
}

void
bcw_cardbus_power(struct bcw_softc *sc, int why)
{
	struct bcw_cardbus_softc *csc = (struct bcw_cardbus_softc *)sc;

	if (why == PWR_RESUME) {
		/* kick the PCI configuration registers */
		bcw_cardbus_setup(csc);
	}
}

void
bcw_cardbus_setup(struct bcw_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcireg_t reg;

	/* program the BAR */
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_BASE0_REG,
	    csc->sc_bar_val);

	/* make sure the right access type is on the cardbus bridge */
	(*cf->cardbus_ctrl)(cc, CARDBUS_MEM_ENABLE);
	(*cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);

	/* enable the appropriate bits in the PCI CSR */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag,
	    CARDBUS_COMMAND_STATUS_REG);
	reg |= CARDBUS_COMMAND_MASTER_ENABLE | CARDBUS_COMMAND_MEM_ENABLE;
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_COMMAND_STATUS_REG,
	    reg);
}

int
bcw_cardbus_enable(struct bcw_softc *sc)
{
	struct bcw_cardbus_softc *csc = (struct bcw_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* power on the socket */
	Cardbus_function_enable(ct);

	/* setup the PCI configuration registers */
	bcw_cardbus_setup(csc);

	/* map and establish the interrupt handler */
	csc->sc_ih = cardbus_intr_establish(cc, cf, csc->sc_intrline, IPL_NET,
	    bcw_intr, sc, sc->sc_dev.dv_xname);
	if (csc->sc_ih == NULL) {
		printf("%s: could not establish interrupt at %d\n",
		    sc->sc_dev.dv_xname, csc->sc_intrline);
		Cardbus_function_disable(ct);
		return (1);
	}

	return (0);
}

void
bcw_cardbus_disable(struct bcw_softc *sc)
{
	struct bcw_cardbus_softc *csc = (struct bcw_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* unhook the interrupt handler */
	cardbus_intr_disestablish(cc, cf, csc->sc_ih);
	csc->sc_ih = NULL;

	/* power down the socket */
	Cardbus_function_disable(ct);
}

void      
bcw_cardbus_conf_write(struct bcw_softc *sc, u_int32_t reg, u_int32_t val)
{          
        Cardbus_conf_write(sc->sc_ca.ca_ct, sc->sc_ca.ca_tag, reg, val);
}       
                
u_int32_t
bcw_cardbus_conf_read(struct bcw_softc *sc, u_int32_t reg)
{
        return Cardbus_conf_read(sc->sc_ca.ca_ct, sc->sc_ca.ca_tag, reg);
}       
