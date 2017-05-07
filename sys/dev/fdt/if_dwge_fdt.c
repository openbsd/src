/*	$OpenBSD: if_dwge_fdt.c,v 1.2 2017/05/07 12:09:46 kettenis Exp $	*/
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

#include <dev/ic/dwc_gmac_var.h>
#include <dev/ic/dwc_gmac_reg.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

int	dwge_fdt_match(struct device *, void *, void *);
void	dwge_fdt_attach(struct device *, struct device *, void *);

struct dwge_fdt_softc {
	struct dwc_gmac_softc	 sc_core;
	void			*sc_ih;
	int			 sc_node;
};

struct cfattach dwge_fdt_ca = {
	sizeof(struct dwge_fdt_softc), dwge_fdt_match, dwge_fdt_attach,
};

void	dwge_fdt_reset_phy(struct dwge_fdt_softc *);
int	dwge_fdt_intr(void *);
void	dwge_fdt_attach_allwinner(struct dwge_fdt_softc *);
void	dwge_fdt_attach_rockchip(struct dwge_fdt_softc *);

int
dwge_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "allwinner,sun7i-a20-gmac") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3399-gmac"));
}

void
dwge_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct dwge_fdt_softc *fsc = (struct dwge_fdt_softc *)self;
	struct dwc_gmac_softc *sc = &fsc->sc_core;
	struct fdt_attach_args *faa = aux;
	int phyloc = MII_PHY_ANY;
	uint32_t phy_supply;
	uint32_t phy;
	int node;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	fsc->sc_node = faa->fa_node;
	sc->sc_bst = faa->fa_iot;
	sc->sc_dmat = faa->fa_dmat;

	if (bus_space_map(sc->sc_bst, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_bsh)) {
		printf(": can't map registers\n");
		return;
	}

	printf("\n");

	/* Lookup PHY. */
	phy = OF_getpropint(faa->fa_node, "phy", 0);
	node = OF_getnodebyphandle(phy);
	if (node)
		phyloc = OF_getpropint(node, "reg", phyloc);

	pinctrl_byname(faa->fa_node, "default");

	/* Do hardware specific initializations. */
	if (OF_is_compatible(faa->fa_node, "allwinner,sun7i-a20-gmac"))
		dwge_fdt_attach_allwinner(fsc);
	else if (OF_is_compatible(faa->fa_node, "rockchip,rk3399-gmac"))
		dwge_fdt_attach_rockchip(fsc);

	/* Enable clock. */
	clock_enable(faa->fa_node, "stmmaceth");
	reset_deassert(faa->fa_node, "stmmaceth");
	delay(5000);

	/* Power up PHY. */
	phy_supply = OF_getpropint(faa->fa_node, "phy-supply", 0);
	if (phy_supply)
		regulator_enable(phy_supply);

	/* Reset PHY */
	dwge_fdt_reset_phy(fsc);

	fsc->sc_ih = arm_intr_establish_fdt(faa->fa_node, IPL_NET,
	    dwge_fdt_intr, sc, sc->sc_dev.dv_xname);
	if (fsc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		goto clrpwr;
	}

	dwc_gmac_attach(sc, GMAC_MII_CLK_150_250M_DIV102, phyloc);

	return;

clrpwr:
	if (phy_supply)
		regulator_disable(phy_supply);
	clock_disable(faa->fa_node, "stmmaceth");
	bus_space_unmap(sc->sc_bst, sc->sc_bsh, faa->fa_reg[0].size);
}

void
dwge_fdt_reset_phy(struct dwge_fdt_softc *sc)
{
	uint32_t *gpio;
	uint32_t delays[3];
	int active = 1;
	int len;

	len = OF_getproplen(sc->sc_node, "snps,reset-gpio");
	if (len <= 0)
		return;

	gpio = malloc(len, M_TEMP, M_WAITOK);

	/* Gather information. */
	OF_getpropintarray(sc->sc_node, "snps,reset-gpio", gpio, len);
	if (OF_getproplen(sc->sc_node, "snps-reset-active-low") == 0)
		active = 0;
	delays[0] = delays[1] = delays[2] = 0;
	OF_getpropintarray(sc->sc_node, "snps,reset-delay-us", delays,
	    sizeof(delays));

	/* Perform reset sequence. */
	gpio_controller_config_pin(gpio, GPIO_CONFIG_OUTPUT);
	gpio_controller_set_pin(gpio, !active);
	delay(delays[0]);
	gpio_controller_set_pin(gpio, active);
	delay(delays[1]);
	gpio_controller_set_pin(gpio, !active);
	delay(delays[2]);

	free(gpio, M_TEMP, len);
}

int
dwge_fdt_intr(void *arg)
{
	struct dwge_fdt_softc *sc = arg;

	return dwc_gmac_intr(&sc->sc_core);
}

/*
 * Allwinner A20/A31.
 */

void
dwge_fdt_attach_allwinner(struct dwge_fdt_softc *sc)
{
	char phy_mode[8];
	uint32_t freq;

	/* default to RGMII */
	OF_getprop(sc->sc_node, "phy-mode", phy_mode, sizeof(phy_mode));
	if (strcmp(phy_mode, "mii") == 0)
		freq = 25000000;
	else
		freq = 125000000;
	clock_set_frequency(sc->sc_node, "allwinner_gmac_tx", freq);
}

/*
 * Rockchip RK3399
 */

/* Registers */
#define RK3399_GRF_SOC_CON5	0xc214
#define  RK3399_GMAC_PHY_INTF_SEL_RGMII	((0x7 << 9) << 16 | (0x1 << 9))
#define  RK3399_GMAC_PHY_INTF_SEL_RMII	((0x7 << 9) << 16 | (0x4 << 9))
#define  RK3399_RMII_MODE_RMII		((1 << 6) << 16 | (1 << 6))
#define  RK3399_RMII_MODE_MII		((1 << 6) << 16 | (0 << 6))
#define RK3399_GRF_SOC_CON6	0xc218
#define  RK3399_GMAC_RXCLK_DLY_ENA	((1 << 15) << 16 | (1 << 15))
#define  RK3399_GMAC_CLK_RX_DL_CFG(val) ((0x7f << 8) << 16 | ((val) << 8))
#define  RK3399_GMAC_TXCLK_DLY_ENA	((1 << 7) << 16 | (1 << 7))
#define  RK3399_GMAC_CLK_TX_DL_CFG(val) ((0x7f << 0) << 16 | ((val) << 0))

void
dwge_fdt_attach_rockchip(struct dwge_fdt_softc *sc)
{
	struct regmap *rm;
	uint32_t grf;
	int tx_delay, rx_delay;

	grf = OF_getpropint(sc->sc_node, "rockchip,grf", 0);
	rm = regmap_byphandle(grf);
	if (rm == NULL)
		return;

	clock_enable(sc->sc_node, "mac_clk_rx");
	clock_enable(sc->sc_node, "mac_clk_tx");
	clock_enable(sc->sc_node, "aclk_mac");
	clock_enable(sc->sc_node, "pclk_mac");

	/* Use RGMII interface. */
	regmap_write_4(rm, RK3399_GRF_SOC_CON5,
	    RK3399_GMAC_PHY_INTF_SEL_RGMII | RK3399_RMII_MODE_MII);

	/* Program clock delay lines. */
	tx_delay = OF_getpropint(sc->sc_node, "tx_delay", 0x30);
	rx_delay = OF_getpropint(sc->sc_node, "rx_delay", 0x10);
	regmap_write_4(rm, RK3399_GRF_SOC_CON6,
	    RK3399_GMAC_TXCLK_DLY_ENA | RK3399_GMAC_CLK_TX_DL_CFG(tx_delay) |
	    RK3399_GMAC_RXCLK_DLY_ENA | RK3399_GMAC_CLK_RX_DL_CFG(rx_delay));
}
