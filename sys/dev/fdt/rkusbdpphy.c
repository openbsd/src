/*	$OpenBSD: rkusbdpphy.c,v 1.1 2025/12/27 15:01:06 patrick Exp $	*/
/*
 * Copyright (c) 2025 Patrick Wildt <patrick@blueri.se>
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
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/* PMA CMN registers */
#define USBDP_COMBO_PHY_REG(idx)		(0x8000 + ((idx) * 4))
/* REG_00A2 */
#define USBDP_COMBO_PHY_LANE_EN(x)		((x) << 0)
#define USBDP_COMBO_PHY_LANE_EN_MASK		(0xf << 0)
#define USBDP_COMBO_PHY_LANE_MUX(x)		((x) << 4)
#define USBDP_COMBO_PHY_LANE_MUX_MASK		(0xf << 4)

/* USB GRF registers */
#define USB_GRF_USB3OTG0_CON1			0x001c
#define USB_GRF_USB3OTG1_CON1			0x0034

#define USBDP_MODE_USB	(1 << 0)
#define USBDP_MODE_DP	(1 << 1)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct rkusbdpphy_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	uint8_t			sc_id;
	uint8_t			sc_available;
	uint8_t			sc_enabled;
	uint8_t			sc_hs;

	struct phy_device	sc_pd;
};

int	rkusbdpphy_match(struct device *, void *, void *);
void	rkusbdpphy_attach(struct device *, struct device *, void *);

const struct cfattach rkusbdpphy_ca = {
	sizeof (struct rkusbdpphy_softc), rkusbdpphy_match, rkusbdpphy_attach
};

struct cfdriver rkusbdpphy_cd = {
	NULL, "rkusbdpphy", DV_DULL
};

int	rkusbdpphy_rk3588_enable(void *, uint32_t *);

int
rkusbdpphy_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;
	int node = faa->fa_node;

	return OF_is_compatible(node, "rockchip,rk3588-usbdp-phy");
}

void
rkusbdpphy_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkusbdpphy_softc *sc = (struct rkusbdpphy_softc *)self;
	struct fdt_attach_args *faa = aux;
	uint32_t reg;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	if (faa->fa_reg[0].addr == 0xfed80000) {
		sc->sc_id = 0;
	} else if (faa->fa_reg[0].addr == 0xfed90000) {
		sc->sc_id = 1;
	} else {
		printf(": unknown register address\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	/*
	 * Some implementations don't have USB3, so if it doesn't
	 * start with super-speed, treat is as HS.
	 */
	if (OF_getproplen(faa->fa_node, "maximum-speed") > 0) {
		char maximum_speed[24];
		OF_getprop(faa->fa_node, "maximum-speed", maximum_speed,
		    sizeof(maximum_speed));
		if (memcmp(maximum_speed, "super-speed",
		    strlen("super-speed")) != 0) {
			sc->sc_hs = 1;
		}
	}

	sc->sc_available |= (USBDP_MODE_DP | USBDP_MODE_USB);
	/* No lane mux, no DP support */
	if (OF_getproplen(faa->fa_node, "rockchip,dp-lane-mux") <= 0)
		sc->sc_available &= ~USBDP_MODE_DP;
	/* All 4 lanes for DP, no USB support */
	if (OF_getproplen(faa->fa_node, "rockchip,dp-lane-mux") ==
	    4 * sizeof(uint32_t))
		sc->sc_available &= ~USBDP_MODE_USB;

	clock_set_assigned(faa->fa_node);
	clock_enable_all(faa->fa_node);
	reset_deassert_all(faa->fa_node);

	/* If lanes are muxed and enabled, DP is on */
	reg = HREAD4(sc, USBDP_COMBO_PHY_REG(0xa2));
	if ((reg & USBDP_COMBO_PHY_LANE_MUX_MASK) &&
	   (reg & USBDP_COMBO_PHY_LANE_EN_MASK)) {
		sc->sc_enabled |= USBDP_MODE_DP;
	}

	printf("\n");

	sc->sc_pd.pd_node = faa->fa_node;
	sc->sc_pd.pd_cookie = sc;
	sc->sc_pd.pd_enable = rkusbdpphy_rk3588_enable;
	phy_register(&sc->sc_pd);
}

int
rkusbdpphy_rk3588_enable(void *cookie, uint32_t *cells)
{
	struct rkusbdpphy_softc *sc = cookie;
	struct regmap *usb_rm;
	int node = sc->sc_pd.pd_node;
	uint32_t type = cells[0];
	uint32_t usb_grf;

	/* We only support USB3 for now. */
	switch (type) {
	case PHY_TYPE_USB3:
		break;
	default:
		return EINVAL;
	}

	usb_grf = OF_getpropint(node, "rockchip,usb-grf", 0);
	usb_rm = regmap_byphandle(usb_grf);
	if (usb_rm == NULL)
		return ENXIO;

	/* No USB or only high-speed, disable and be done */
	if (!(sc->sc_available & USBDP_MODE_USB) || sc->sc_hs) {
		regmap_write_4(usb_rm,
		    sc->sc_id ? USB_GRF_USB3OTG1_CON1 : USB_GRF_USB3OTG0_CON1,
		    0xffff1100);
		return 0;
	}

	if (!sc->sc_enabled) {
		/* Enable USB */
		regmap_write_4(usb_rm,
		    sc->sc_id ? USB_GRF_USB3OTG1_CON1 : USB_GRF_USB3OTG0_CON1,
		    0xffff0188);
		sc->sc_enabled |= USBDP_MODE_USB;
	}

	return 0;
}
