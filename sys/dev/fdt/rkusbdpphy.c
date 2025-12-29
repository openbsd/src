/*	$OpenBSD: rkusbdpphy.c,v 1.2 2025/12/29 10:21:22 patrick Exp $	*/
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
/* REG_00E3 */
#define USBDP_COMBO_PHY_INIT_RSTN		(1 << 3)

/* USB GRF registers */
#define USB_GRF_USB3OTG0_CON1			0x001c
#define USB_GRF_USB3OTG1_CON1			0x0034

/* USBDPPHY GRF registers */
#define USBDPPHY_GRF_CON1			0x0004

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
int	rkusbdpphy_rk3588_init(struct rkusbdpphy_softc *);

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
		    0xffff0188);
		return 0;
	}

	if (!sc->sc_enabled) {
		/* Init sequence */
		if (rkusbdpphy_rk3588_init(sc) != 0)
			return ENXIO;

		/* Enable USB */
		regmap_write_4(usb_rm,
		    sc->sc_id ? USB_GRF_USB3OTG1_CON1 : USB_GRF_USB3OTG0_CON1,
		    0xffff1100);
		sc->sc_enabled |= USBDP_MODE_USB;
	}

	return 0;
}

struct rkusbdpphy_init_reg {
	uint16_t reg, val;
};

const struct rkusbdpphy_init_reg rkusbdpphy_init_regs[] = {
	{ 0x041, 0x44 }, { 0x08d, 0xe8 }, { 0x092, 0x44 }, { 0x0a3, 0x18 },
	{ 0x207, 0xe5 }, { 0x21e, 0x00 }, { 0x265, 0x1c }, { 0x2bc, 0x00 },
	{ 0x607, 0xe5 }, { 0x61e, 0x00 }, { 0x665, 0x1c }, { 0x6bc, 0x00 },
	{ 0x10a, 0x60 }, { 0x356, 0x33 }, { 0x756, 0x33 }, { 0x264, 0x74 },
	{ 0x359, 0x17 }, { 0x232, 0x13 }, { 0x664, 0x74 }, { 0x759, 0x17 },
	{ 0x632, 0x13 }, { 0x364, 0x40 }, { 0x36a, 0x40 }, { 0x370, 0x40 },
	{ 0x376, 0x40 }, { 0x764, 0x40 }, { 0x76a, 0x40 }, { 0x770, 0x40 },
	{ 0x776, 0x40 }, { 0x0f0, 0x30 }, { 0x0f1, 0x06 }, { 0x384, 0x00 },
	{ 0x784, 0x00 }, { 0x10f, 0x0f }, { 0x34b, 0xff }, { 0x74b, 0xff },
	{ 0x34d, 0x0f }, { 0x74d, 0x0f }, { 0x23f, 0x2a }, { 0x245, 0x28 },
	{ 0x28c, 0x03 }, { 0x38e, 0x03 }, { 0x3b3, 0x27 }, { 0x3b4, 0x22 },
	{ 0x3b5, 0x26 }, { 0x63f, 0x2a }, { 0x645, 0x28 }, { 0x68c, 0x03 },
	{ 0x78e, 0x03 }, { 0x7b3, 0x27 }, { 0x7b4, 0x22 }, { 0x7b5, 0x26 },
	{ 0x012, 0x0f }, { 0x018, 0x3c }, { 0x019, 0xf7 }, { 0x01b, 0x20 },
	{ 0x01c, 0x7d }, { 0x01d, 0x68 }, { 0x2bd, 0x1a }, { 0x6bd, 0x1a },
	{ 0x110, 0x3f }, { 0x435, 0x08 }, { 0x835, 0x08 }, { 0x035, 0x30 },
	{ 0x9, 0x6e },
};

const struct rkusbdpphy_init_reg rkusbdpphy_24m_refclk_init_regs[] = {
	{ 0x024, 0x68 }, { 0x025, 0x68 }, { 0x04a, 0x24 }, { 0x04b, 0x44 },
	{ 0x04c, 0x3f }, { 0x04d, 0x44 }, { 0x057, 0xa9 }, { 0x058, 0x71 },
	{ 0x059, 0x71 }, { 0x05a, 0xa9 }, { 0x05d, 0xa9 }, { 0x05e, 0x71 },
	{ 0x05f, 0x71 }, { 0x060, 0xa9 }, { 0x063, 0x41 }, { 0x064, 0x00 },
	{ 0x065, 0x05 }, { 0x06b, 0x2a }, { 0x06c, 0x17 }, { 0x06d, 0x17 },
	{ 0x06e, 0x2a }, { 0x072, 0x04 }, { 0x073, 0x08 }, { 0x074, 0x08 },
	{ 0x075, 0x04 }, { 0x076, 0x20 }, { 0x077, 0x01 }, { 0x078, 0x09 },
	{ 0x079, 0x03 }, { 0x07c, 0x29 }, { 0x07d, 0x02 }, { 0x07e, 0x02 },
	{ 0x07f, 0x29 }, { 0x082, 0x2a }, { 0x083, 0x17 }, { 0x084, 0x17 },
	{ 0x085, 0x2a }, { 0x089, 0x20 }, { 0x0fc, 0x0a }, { 0x0fd, 0x07 },
	{ 0x0fe, 0x07 }, { 0x0ff, 0x0c }, { 0x101, 0x12 }, { 0x102, 0x1a },
	{ 0x103, 0x1a }, { 0x104, 0x3f }, { 0x338, 0x68 }, { 0x33a, 0xd0 },
	{ 0x33c, 0x87 }, { 0x33e, 0x70 }, { 0x340, 0x70 }, { 0x342, 0xa9 },
	{ 0x738, 0x68 }, { 0x73a, 0xd0 }, { 0x73c, 0x87 }, { 0x73e, 0x70 },
	{ 0x740, 0x70 }, { 0x742, 0xa9 }, { 0x28f, 0xd0 }, { 0x291, 0xd0 },
	{ 0x292, 0x01 }, { 0x293, 0x0d }, { 0x295, 0xe0 }, { 0x297, 0xe0 },
	{ 0x299, 0xa8 }, { 0x68f, 0xd0 }, { 0x691, 0xd0 }, { 0x692, 0x01 },
	{ 0x693, 0x0d }, { 0x695, 0xe0 }, { 0x697, 0xe0 }, { 0x699, 0xa8 },
};

const struct rkusbdpphy_init_reg rkusbdpphy_26m_refclk_init_regs[] = {
	{ 0x20c, 0x07 }, { 0x217, 0x80 }, { 0x40c, 0x07 }, { 0x417, 0x80 },
	{ 0x60c, 0x07 }, { 0x617, 0x80 }, { 0x80c, 0x07 }, { 0x817, 0x80 },
	{ 0x08a, 0x38 }, { 0x041, 0x44 }, { 0x092, 0x44 }, { 0x0e3, 0x02 },
	{ 0x21e, 0x04 }, { 0x61e, 0x04 }, { 0x226, 0x77 }, { 0x626, 0x77 },
	{ 0x015, 0x01 }, { 0x038, 0x38 }, { 0x018, 0x24 }, { 0x019, 0x77 },
	{ 0x01c, 0x76 }, { 0x08d, 0xe8 }, { 0x2bd, 0x15 }, { 0x6bd, 0x15 },
	{ 0x207, 0xe5 }, { 0x607, 0xe5 }, { 0x267, 0x48 }, { 0x667, 0x48 },
	{ 0x269, 0x07 }, { 0x26a, 0x22 }, { 0x669, 0x07 }, { 0x66a, 0x22 },
	{ 0x26e, 0x3e }, { 0x66e, 0x3e }, { 0x279, 0x02 }, { 0x679, 0x02 },
	{ 0x28d, 0x1e }, { 0x68d, 0x1e }, { 0x2a6, 0x2f }, { 0x6a6, 0x2f },
	{ 0x30c, 0x0e }, { 0x312, 0x06 }, { 0x70c, 0x0e }, { 0x712, 0x06 },
	{ 0x0a3, 0x18 }, { 0x2bc, 0x00 }, { 0x6bc, 0x00 }
};

int
rkusbdpphy_rk3588_init(struct rkusbdpphy_softc *sc)
{
	struct regmap *udphy_rm;
	uint32_t freq, udphy_grf;
	int i, node = sc->sc_pd.pd_node;

	udphy_grf = OF_getpropint(node, "rockchip,usbdpphy-grf", 0);
	udphy_rm = regmap_byphandle(udphy_grf);
	if (udphy_rm == NULL)
		return ENXIO;

	reset_assert_all(node);
	delay(10000);

	if (sc->sc_available & USBDP_MODE_USB) {
		/* RX LPFS Enable */
		regmap_write_4(udphy_rm, USBDPPHY_GRF_CON1, 0x40004000);
	}

	/* PMA Block Power On */
	regmap_write_4(udphy_rm, USBDPPHY_GRF_CON1, 0x20002000);

	reset_deassert(node, "pma_apb");
	reset_deassert(node, "pcs_apb");

	for (i = 0; i < nitems(rkusbdpphy_init_regs); i++) {
		HWRITE4(sc, USBDP_COMBO_PHY_REG(
		    rkusbdpphy_init_regs[i].reg),
		    rkusbdpphy_init_regs[i].val);
	}

	freq = clock_get_frequency(node, "refclk");
	switch (freq) {
	case 24000000:
		for (i = 0; i < nitems(rkusbdpphy_24m_refclk_init_regs); i++) {
			HWRITE4(sc, USBDP_COMBO_PHY_REG(
			    rkusbdpphy_24m_refclk_init_regs[i].reg),
			    rkusbdpphy_24m_refclk_init_regs[i].val);
		}
		break;
	case 26000000:
		for (i = 0; i < nitems(rkusbdpphy_26m_refclk_init_regs); i++) {
			HWRITE4(sc, USBDP_COMBO_PHY_REG(
			    rkusbdpphy_26m_refclk_init_regs[i].reg),
			    rkusbdpphy_26m_refclk_init_regs[i].val);
		}
		break;
	}

	/* TODO: configure lanes for DP */

	if (sc->sc_available & USBDP_MODE_USB) {
		reset_deassert(node, "init");
	}

	if (sc->sc_available & USBDP_MODE_DP) {
		HSET4(sc, USBDP_COMBO_PHY_REG(0xe3), USBDP_COMBO_PHY_INIT_RSTN);
	}

	delay(1);

	if (sc->sc_available & USBDP_MODE_USB) {
		reset_deassert(node, "cmn");
		reset_deassert(node, "lane");
	}

	/* TODO: wait for PLL lock? */
	delay(100000);

	return 0;
}
