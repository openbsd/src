/*	$OpenBSD: if_dwge_fdt.c,v 1.1 2016/08/13 22:07:01 kettenis Exp $	*/
/*
 * Copyright (c) 2016 Patrick Wildt <patrick@blueri.se>
 * Copyright (c) 2016 Mark Kettenis <kettenis@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <net/if.h>
#include <net/if_media.h>
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/sunxi/sunxireg.h>
#include <armv7/sunxi/sxiccmuvar.h>

#include <dev/ic/dwc_gmac_var.h>
#include <dev/ic/dwc_gmac_reg.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

int	dwge_fdt_match(struct device *, void *, void *);
void	dwge_fdt_attach(struct device *, struct device *, void *);
int	dwge_fdt_intr(void *);

struct dwge_fdt_softc {
	struct dwc_gmac_softc	 sc_core;
	void			*sc_ih;
};

struct cfattach dwge_fdt_ca = {
	sizeof(struct dwge_fdt_softc), dwge_fdt_match, dwge_fdt_attach,
};

int
dwge_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "allwinner,sun7i-a20-gmac");
}

void
dwge_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct dwge_fdt_softc *fsc = (struct dwge_fdt_softc *)self;
	struct dwc_gmac_softc *sc = &fsc->sc_core;
	struct fdt_attach_args *faa = aux;
	char phy_mode[8];
	uint32_t phy_supply;
	int clock;

	if (faa->fa_nreg < 1)
		return;

	pinctrl_byname(faa->fa_node, "default");

	sc->sc_bst = faa->fa_iot;
	sc->sc_dmat = faa->fa_dmat;

	printf("\n");

	if (bus_space_map(sc->sc_bst, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_bsh))
		panic("%s: bus_space_map failed!", __func__);

	/* default to RGMII */
	OF_getprop(faa->fa_node, "phy-mode", phy_mode, sizeof(phy_mode));
	if (strcmp(phy_mode, "mii") == 0)
		clock = CCMU_GMAC_MII;
	else
		clock = CCMU_GMAC_RGMII;

	/* enable clock */
	sxiccmu_enablemodule(clock);
	delay(5000);

	/* power up phy */
	phy_supply = OF_getpropint(faa->fa_node, "phy-supply", 0);
	if (phy_supply)
		regulator_enable(phy_supply);

	fsc->sc_ih = arm_intr_establish_fdt(faa->fa_node, IPL_NET,
	    dwge_fdt_intr, sc, sc->sc_dev.dv_xname);
	if (fsc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		goto clrpwr;
	}

	dwc_gmac_attach(sc, GMAC_MII_CLK_150_250M_DIV102);

	return;
clrpwr:
	if (phy_supply)
		regulator_disable(phy_supply);
	sxiccmu_disablemodule(clock);
	bus_space_unmap(sc->sc_bst, sc->sc_bsh, faa->fa_reg[0].size);
}

int
dwge_fdt_intr(void *arg)
{
	struct dwge_fdt_softc *sc = arg;

	return dwc_gmac_intr(&sc->sc_core);
}
