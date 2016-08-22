/*	$OpenBSD: sxiccmu.c,v 1.12 2016/08/22 19:29:32 kettenis Exp $	*/
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2013 Artturi Alm
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/device.h>

#include <arm/cpufunc.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/sunxi/sunxireg.h>
#include <armv7/sunxi/sxiccmuvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>

#ifdef DEBUG_CCMU
#define DPRINTF(x)	do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

#define	CCMU_SCLK_GATING		(1U << 31)
#define	CCMU_GET_CLK_DIV_RATIO_N(x)	(((x) >> 16) & 0x03)
#define	CCMU_GET_CLK_DIV_RATIO_M(x)	((x) & 0x07)

#define	CCMU_PLL6_CFG			0x28
#define	CCMU_PLL6_EN			(1U << 31)
#define	CCMU_PLL6_BYPASS_EN		(1 << 30)
#define	CCMU_PLL6_SATA_CLK_EN		(1 << 14)
#define	CCMU_PLL6_FACTOR_N		(31 << 8)
#define	CCMU_PLL6_FACTOR_K		(3 << 4)
#define	CCMU_PLL6_FACTOR_M		(3 << 0)

#define	CCMU_AHB_GATING0		0x60
#define	CCMU_AHB_GATING_USB0		(1 << 0)
#define	CCMU_AHB_GATING_EHCI0		(1 << 1)
#define	CCMU_AHB_GATING_OHCI0		(1 << 2)
#define	CCMU_AHB_GATING_EHCI1		(1 << 3)
#define	CCMU_AHB_GATING_OHCI1		(1 << 4)
#define	CCMU_AHB_GATING_SS		(1 << 5)
#define	CCMU_AHB_GATING_DMA		(1 << 6)
#define	CCMU_AHB_GATING_BIST		(1 << 7)
#define	CCMU_AHB_GATING_SDMMCx(x)	(1 << (8 + (x)))
#define	CCMU_AHB_GATING_NAND		(1 << 13)
#define	CCMU_AHB_GATING_EMAC		(1 << 17)
#define	CCMU_AHB_GATING_SATA		(1 << 25)

#define	CCMU_AHB_GATING1		0x64
#define	CCMU_AHB_GATING_MALI400		(1 << 20)
#define	CCMU_AHB_GATING_MP		(1 << 18)
#define	CCMU_AHB_GATING_GMAC		(1 << 17)
#define	CCMU_AHB_GATING_DE_FE1		(1 << 15)
#define	CCMU_AHB_GATING_DE_FE0		(1 << 14)
#define	CCMU_AHB_GATING_DE_BE1		(1 << 13)
#define	CCMU_AHB_GATING_DE_BE0		(1 << 12)
#define	CCMU_AHB_GATING_HDMI		(1 << 11)
#define	CCMU_AHB_GATING_CSI1		(1 << 9)
#define	CCMU_AHB_GATING_CSI0		(1 << 8)
#define	CCMU_AHB_GATING_LCD1		(1 << 5)
#define	CCMU_AHB_GATING_LCD0		(1 << 4)
#define	CCMU_AHB_GATING_TVE1		(1 << 3)
#define	CCMU_AHB_GATING_TVE0		(1 << 2)
#define	CCMU_AHB_GATING_TVD		(1 << 1)
#define	CCMU_AHB_GATING_VE		(1 << 0)

#define	CCMU_APB_GATING0		0x68
#define	CCMU_APB_GATING_PIO		(1 << 5)
#define	CCMU_APB_GATING1		0x6c
#define	CCMU_APB_GATING_UARTx(x)	(1 << (16 + (x)))
#define	CCMU_APB_GATING_TWIx(x)		(1 << (x))
#define	CCMU_APB_GATING_TWI4		(1 << 15)

#define	CCMU_NAND_CLK			0x80
#define	CCMU_NAND_CLK_SRC_GATING_OSC24M	(0 << 24)
#define	CCMU_NAND_CLK_SRC_GATING_PLL6	(1 << 24)
#define	CCMU_NAND_CLK_SRC_GATING_PLL5	(2 << 24)
#define	CCMU_NAND_CLK_SRC_GATING_MASK	(3 << 24)

#define CCMU_SDx_CLK(x)			(0x88 + (x) * 4)
#define CCMU_SDx_CLK_GATING		(1U << 31)
#define CCMU_SDx_CLK_SRC_GATING_OSC24M	(0 << 24)
#define CCMU_SDx_CLK_SRC_GATING_PLL6	(1 << 24)
#define CCMU_SDx_CLK_SRC_GATING_PLL5	(2 << 24)
#define CCMU_SDx_CLK_SRC_GATING_MASK	(3 << 24)
#define CCMU_SDx_CLK_FACTOR_N		(3 << 16)
#define CCMU_SDx_CLK_FACTOR_N_SHIFT	16
#define CCMU_SDx_CLK_FACTOR_M		(7 << 0)
#define CCMU_SDx_CLK_FACTOR_M_SHIFT	0

#define	CCMU_SATA_CLK			0xc8
#define	CCMU_SATA_CLK_SRC_GATING	(1 << 24)

#define	CCMU_USB_CLK			0xcc
#define	CCMU_USB_PHY			(1 << 8)
#define	CCMU_SCLK_GATING_OHCI1		(1 << 7)
#define	CCMU_SCLK_GATING_OHCI0		(1 << 6)
#define	CCMU_OHCI_CLK_SRC		(1 << 4)
#define	CCMU_USB2_RESET			(1 << 2)
#define	CCMU_USB1_RESET			(1 << 1)
#define	CCMU_USB0_RESET			(1 << 0)

#define	CCMU_GMAC_CLK_REG		0x164
#define	CCMU_GMAC_CLK_TXC_DIV		(0x3 << 8)
#define	CCMU_GMAC_CLK_TXC_DIV_1000	0
#define	CCMU_GMAC_CLK_TXC_DIV_100	1
#define	CCMU_GMAC_CLK_TXC_DIV_10	2
#define	CCMU_GMAC_CLK_RXDC		(0x7 << 5)
#define	CCMU_GMAC_CLK_RXIE		(1 << 4)
#define	CCMU_GMAC_CLK_TXIE		(1 << 3)
#define	CCMU_GMAC_CLK_PIT		(1 << 2)
#define	CCMU_GMAC_CLK_TCS		(0x3 << 0)
#define	CCMU_GMAC_CLK_TCS_MII		0
#define	CCMU_GMAC_CLK_TCS_EXT_125	1
#define	CCMU_GMAC_CLK_TCS_INT_RGMII	2

struct sxiccmu_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

void	sxiccmu_attach(struct device *, struct device *, void *);
void	sxiccmu_enablemodule(int);

struct cfattach	sxiccmu_ca = {
	sizeof (struct sxiccmu_softc), NULL, sxiccmu_attach
};

struct cfdriver sxiccmu_cd = {
	NULL, "sxiccmu", DV_DULL
};

void sxiccmu_attach_clock(struct sxiccmu_softc *, int);

void
sxiccmu_attach(struct device *parent, struct device *self, void *args)
{
	struct sxiccmu_softc *sc = (struct sxiccmu_softc *)self;
	struct armv7_attach_args *aa = args;
	int node;

	sc->sc_iot = aa->aa_iot;

	if (bus_space_map(sc->sc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("sxiccmu_attach: bus_space_map failed!");

	node = OF_finddevice("/clocks");
	if (node == -1)
		panic("%s: can't find clocks", __func__);

	printf("\n");

	for (node = OF_child(node); node; node = OF_peer(node))
		sxiccmu_attach_clock(sc, node);
}

struct sxiccmu_clock {
	int sc_node;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct clock_device sc_cd;
	struct reset_device sc_rd;
};

struct sxiccmu_device {
	const char *compat;
	uint32_t (*get_frequency)(void *, uint32_t *);
	int	(*set_frequency)(void *, uint32_t *, uint32_t);
	void	(*enable)(void *, uint32_t *, int);
	void	(*reset)(void *, uint32_t *, int);
};

uint32_t sxiccmu_gen_get_frequency(void *, uint32_t *);
uint32_t sxiccmu_osc_get_frequency(void *, uint32_t *);
uint32_t sxiccmu_pll6_get_frequency(void *, uint32_t *);
void	sxiccmu_pll6_enable(void *, uint32_t *, int);
uint32_t sxiccmu_apb1_get_frequency(void *, uint32_t *);
int	sxiccmu_gmac_set_frequency(void *, uint32_t *, uint32_t);
void	sxiccmu_gate_enable(void *, uint32_t *, int);
void	sxiccmu_reset(void *, uint32_t *, int);

struct sxiccmu_device sxiccmu_devices[] = {
	{
		.compat = "allwinner,sun4i-a10-osc-clk",
		.get_frequency = sxiccmu_osc_get_frequency,
	},
	{
		.compat = "allwinner,sun4i-a10-pll6-clk",
		.get_frequency = sxiccmu_pll6_get_frequency,
		.enable = sxiccmu_pll6_enable
	},
	{
		.compat = "allwinner,sun4i-a10-apb1-clk",
		.get_frequency = sxiccmu_apb1_get_frequency,
	},
	{
		.compat = "allwinner,sun4i-a10-ahb-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun4i-a10-apb0-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun4i-a10-apb1-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun4i-a10-usb-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable,
		.reset = sxiccmu_reset
	},
	{
		.compat = "allwinner,sun5i-a10s-ahb-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun5i-a10s-apb0-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun5i-a10s-apb1-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun5i-a13-ahb-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun5i-a13-apb0-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun5i-a13-apb1-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun7i-a20-ahb-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun7i-a20-apb0-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun7i-a20-apb1-gates-clk",
		.get_frequency = sxiccmu_gen_get_frequency,
		.enable = sxiccmu_gate_enable
	},
	{
		.compat = "allwinner,sun7i-a20-gmac-clk",
		.set_frequency = sxiccmu_gmac_set_frequency
	},
};

void
sxiccmu_attach_clock(struct sxiccmu_softc *sc, int node)
{
	struct sxiccmu_clock *clock;
	uint32_t reg[2];
	int i;

	for (i = 0; i < nitems(sxiccmu_devices); i++)
		if (OF_is_compatible(node, sxiccmu_devices[i].compat))
			break;
	if (i == nitems(sxiccmu_devices))
		return;

	clock = malloc(sizeof(*clock), M_DEVBUF, M_WAITOK);
	clock->sc_node = node;

	if (OF_getpropintarray(node, "reg", reg, sizeof(reg)) == sizeof(reg)) {
		clock->sc_iot = sc->sc_iot;
		if (bus_space_map(clock->sc_iot, reg[0], reg[1], 0,
		    &clock->sc_ioh)) {
			printf("%s: can't map registers", sc->sc_dev.dv_xname);
			free(clock, M_DEVBUF, sizeof(*clock));
			return;
		}
	}

	clock->sc_cd.cd_node = node;
	clock->sc_cd.cd_cookie = clock;
	clock->sc_cd.cd_get_frequency = sxiccmu_devices[i].get_frequency;
	clock->sc_cd.cd_set_frequency = sxiccmu_devices[i].set_frequency;
	clock->sc_cd.cd_enable = sxiccmu_devices[i].enable;
	clock_register(&clock->sc_cd);

	if (sxiccmu_devices[i].reset) {
		clock->sc_rd.rd_node = node;
		clock->sc_rd.rd_cookie = clock;
		clock->sc_rd.rd_reset = sxiccmu_devices[i].reset;
		reset_register(&clock->sc_rd);
	}
}

/*
 * A "generic" function that simply gets the clock frequency from the
 * parent clock.  Useful for clock gating devices that don't scale the
 * their clocks.
 */
uint32_t
sxiccmu_gen_get_frequency(void *cookie, uint32_t *cells)
{
	struct sxiccmu_clock *sc = cookie;

	return clock_get_frequency(sc->sc_node, NULL);
}

uint32_t
sxiccmu_osc_get_frequency(void *cookie, uint32_t *cells)
{
	struct sxiccmu_clock *sc = cookie;

	return OF_getpropint(sc->sc_node, "clock-frequency", 24000000);
}

#define CCU_PLL6_ENABLE			(1U << 31)
#define CCU_PLL6_BYPASS_EN		(1U << 30)
#define CCU_PLL6_SATA_CLK_EN		(1U << 14)
#define CCU_PLL6_FACTOR_N(x)		(((x) >> 8) & 0x1f)
#define CCU_PLL6_FACTOR_N_MASK		(0x1f << 8)
#define CCU_PLL6_FACTOR_N_SHIFT		8
#define CCU_PLL6_FACTOR_K(x)		(((x) >> 4) & 0x3)
#define CCU_PLL6_FACTOR_K_MASK		(0x3 << 4)
#define CCU_PLL6_FACTOR_K_SHIFT		4
#define CCU_PLL6_FACTOR_M(x)		(((x) >> 0) & 0x3)
#define CCU_PLL6_FACTOR_M_MASK		(0x3 << 0)
#define CCU_PLL6_FACTOR_M_SHIFT		0

uint32_t
sxiccmu_pll6_get_frequency(void *cookie, uint32_t *cells)
{
	struct sxiccmu_clock *sc = cookie;
	uint32_t reg, k, m, n, freq;
	uint32_t idx = cells[0];

	/* XXX Assume bypass is disabled. */
	reg = SXIREAD4(sc, 0);
	k = CCU_PLL6_FACTOR_K(reg) + 1;
	m = CCU_PLL6_FACTOR_M(reg) + 1;
	n = CCU_PLL6_FACTOR_N(reg);

	freq = clock_get_frequency_idx(sc->sc_node, 0);
	switch (idx) {
	case 0:	
		return (freq * n * k) / m / 6;		/* pll6_sata */
	case 1:	
		return (freq * n * k) / m / 2;		/* pll6_other */
	case 2:	
		return (freq * n * k) / m;		/* pll6 */
	case 3:
		return (freq * n * k) / m / 4;		/* pll6_div_4 */
	}

	return 0;
}

void
sxiccmu_pll6_enable(void *cookie, uint32_t *cells, int on)
{
	struct sxiccmu_clock *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t reg;

	/* 
	 * Since this clock has several outputs, we never turn it off.
	 */

	reg = SXIREAD4(sc, 0);
	switch (idx) {
	case 0:			/* pll6_sata */
		if (on)
			reg |= CCU_PLL6_SATA_CLK_EN;
		else
			reg &= ~CCU_PLL6_SATA_CLK_EN;
		/* FALLTHROUGH */
	case 1:			/* pll6_other */
	case 2:			/* pll6 */
	case 3:			/* pll6_div_4 */
		if (on)
			reg |= CCU_PLL6_ENABLE;
	}
	SXIWRITE4(sc, 0, reg);
}

#define CCU_APB1_CLK_RAT_N(x)		(((x) >> 16) & 0x3)
#define CCU_APB1_CLK_RAT_M(x)		(((x) >> 0) & 0x1f)
#define CCU_APB1_CLK_SRC_SEL(x)		(((x) >> 24) & 0x3)

uint32_t
sxiccmu_apb1_get_frequency(void *cookie, uint32_t *cells)
{
	struct sxiccmu_clock *sc = cookie;
	uint32_t reg, m, n, freq;
	int idx;

	reg = SXIREAD4(sc, 0);
	m = CCU_APB1_CLK_RAT_M(reg);
	n = CCU_APB1_CLK_RAT_N(reg);
	idx = CCU_APB1_CLK_SRC_SEL(reg);

	freq = clock_get_frequency_idx(sc->sc_node, idx);
	return freq / (1 << n) / (m + 1);
}

#define	CCU_GMAC_CLK_PIT		(1 << 2)
#define	CCU_GMAC_CLK_TCS		(3 << 0)
#define	CCU_GMAC_CLK_TCS_MII		0
#define	CCU_GMAC_CLK_TCS_EXT_125	1
#define	CCU_GMAC_CLK_TCS_INT_RGMII	2

int
sxiccmu_gmac_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	struct sxiccmu_clock *sc = cookie;

	switch (freq) {
	case 25000000:		/* MMI, 25 MHz */
		SXICMS4(sc, 0, CCU_GMAC_CLK_PIT|CCU_GMAC_CLK_TCS,
		    CCU_GMAC_CLK_TCS_MII);
		break;
	case 125000000:		/* RGMII, 125 MHz */
		SXICMS4(sc, 0, CCU_GMAC_CLK_PIT|CCU_GMAC_CLK_TCS,
		    CCU_GMAC_CLK_PIT|CCU_GMAC_CLK_TCS_INT_RGMII);
		break;
	default:
		return -1;
	}

	return 0;
}

void
sxiccmu_gate_enable(void *cookie, uint32_t *cells, int on)
{
	struct sxiccmu_clock *sc = cookie;
	int reg = cells[0] / 32;
	int bit = cells[0] % 32;

	if (on)
		SXISET4(sc, reg * 4, (1U << bit));
	else
		SXICLR4(sc, reg * 4, (1U << bit));
}

void
sxiccmu_reset(void *cookie, uint32_t *cells, int assert)
{
	struct sxiccmu_clock *sc = cookie;
	int reg = cells[0] / 32;
	int bit = cells[0] % 32;

	if (assert)
		SXICLR4(sc, reg * 4, (1U << bit));
	else
		SXISET4(sc, reg * 4, (1U << bit));
}

/* XXX spl? */
void
sxiccmu_enablemodule(int mod)
{
	struct sxiccmu_softc *sc = sxiccmu_cd.cd_devs[0];
	uint32_t reg;

	DPRINTF(("\nsxiccmu_enablemodule: mod %d\n", mod));

	/* XXX reorder? */
	switch (mod) {
	case CCMU_EHCI0:
		SXISET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_EHCI0);
		SXISET4(sc, CCMU_USB_CLK, CCMU_USB1_RESET | CCMU_USB_PHY);
		break;
	case CCMU_EHCI1:
		SXISET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_EHCI1);
		SXISET4(sc, CCMU_USB_CLK, CCMU_USB2_RESET | CCMU_USB_PHY);
		break;
	case CCMU_OHCI0:
		SXISET4(sc, CCMU_USB_CLK, CCMU_OHCI_CLK_SRC);
		SXISET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_OHCI0);
		SXISET4(sc, CCMU_USB_CLK, CCMU_SCLK_GATING_OHCI0);
		break;
	case CCMU_OHCI1:
		SXISET4(sc, CCMU_USB_CLK, CCMU_OHCI_CLK_SRC);
		SXISET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_OHCI1);
		SXISET4(sc, CCMU_USB_CLK, CCMU_SCLK_GATING_OHCI1);
		break;
	case CCMU_AHCI:
		reg = SXIREAD4(sc, CCMU_PLL6_CFG);
		reg &= ~(CCMU_PLL6_BYPASS_EN | CCMU_PLL6_FACTOR_M |
		    CCMU_PLL6_FACTOR_N);
		reg |= CCMU_PLL6_EN | CCMU_PLL6_SATA_CLK_EN;
		reg |= 25 << 8;
		reg |= (reg >> 4 & 3);
		SXIWRITE4(sc, CCMU_PLL6_CFG, reg);

		SXISET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_SATA);
		delay(1000);

		SXIWRITE4(sc, CCMU_SATA_CLK, CCMU_SCLK_GATING);
		break;
	case CCMU_EMAC:
		SXISET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_EMAC);
		break;
	case CCMU_GMAC_MII:
		SXISET4(sc, CCMU_AHB_GATING1, CCMU_AHB_GATING_GMAC);
		SXICMS4(sc, CCMU_GMAC_CLK_REG,
		    CCMU_GMAC_CLK_PIT|CCMU_GMAC_CLK_TCS,
		    CCMU_GMAC_CLK_TCS_MII);
		break;
	case CCMU_GMAC_RGMII:
		SXISET4(sc, CCMU_AHB_GATING1, CCMU_AHB_GATING_GMAC);
		SXICMS4(sc, CCMU_GMAC_CLK_REG,
		    CCMU_GMAC_CLK_PIT|CCMU_GMAC_CLK_TCS,
		    CCMU_GMAC_CLK_PIT|CCMU_GMAC_CLK_TCS_INT_RGMII);
		break;
	case CCMU_DMA:
		SXISET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_DMA);
		break;
	case CCMU_UART0:
	case CCMU_UART1:
	case CCMU_UART2:
	case CCMU_UART3:
	case CCMU_UART4:
	case CCMU_UART5:
	case CCMU_UART6:
	case CCMU_UART7:
		SXISET4(sc, CCMU_APB_GATING1,
		    CCMU_APB_GATING_UARTx(mod - CCMU_UART0));
		break;
	case CCMU_SDMMC0:
	case CCMU_SDMMC1:
	case CCMU_SDMMC2:
	case CCMU_SDMMC3:
		SXISET4(sc, CCMU_AHB_GATING0,
		    CCMU_AHB_GATING_SDMMCx(mod - CCMU_SDMMC0));
		break;
	case CCMU_TWI0:
	case CCMU_TWI1:
	case CCMU_TWI2:
	case CCMU_TWI3:
	case CCMU_TWI4:
		SXISET4(sc, CCMU_APB_GATING1, mod == CCMU_TWI4
		    ? CCMU_APB_GATING_TWI4
		    : CCMU_APB_GATING_TWIx(mod - CCMU_TWI0));
		break;
	case CCMU_PIO:
		SXISET4(sc, CCMU_APB_GATING0, CCMU_APB_GATING_PIO);
		break;
	default:
		break;
	}
}

void
sxiccmu_disablemodule(int mod)
{
	struct sxiccmu_softc *sc = sxiccmu_cd.cd_devs[0];

	DPRINTF(("\nsxiccmu_disablemodule: mod %d\n", mod));

	switch (mod) {
	case CCMU_EHCI0:
		SXICLR4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_EHCI0);
		SXICLR4(sc, CCMU_USB_CLK, CCMU_USB1_RESET | CCMU_USB_PHY);
		break;
	case CCMU_EHCI1:
		SXICLR4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_EHCI1);
		SXICLR4(sc, CCMU_USB_CLK, CCMU_USB2_RESET | CCMU_USB_PHY);
		break;
	case CCMU_OHCI0:
		SXICLR4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_OHCI0);
		SXICLR4(sc, CCMU_USB_CLK, CCMU_SCLK_GATING_OHCI0);
	case CCMU_OHCI1:
		SXICLR4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_OHCI1);
		SXICLR4(sc, CCMU_USB_CLK, CCMU_SCLK_GATING_OHCI1);
		break;
	case CCMU_AHCI:
		/* XXX possibly wrong */
		SXICLR4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_SATA);
		SXIWRITE4(sc, CCMU_SATA_CLK, 0);
		break;
	case CCMU_EMAC:
		SXICLR4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_EMAC);
		break;
	case CCMU_GMAC_MII:
	case CCMU_GMAC_RGMII:
		SXICLR4(sc, CCMU_AHB_GATING1, CCMU_AHB_GATING_GMAC);
		break;
	case CCMU_DMA:
		SXICLR4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_DMA);
		break;
	case CCMU_UART0:
	case CCMU_UART1:
	case CCMU_UART2:
	case CCMU_UART3:
	case CCMU_UART4:
	case CCMU_UART5:
	case CCMU_UART6:
	case CCMU_UART7:
		SXICLR4(sc, CCMU_APB_GATING1,
		    CCMU_APB_GATING_UARTx(mod - CCMU_UART0));
		break;
	case CCMU_SDMMC0:
	case CCMU_SDMMC1:
	case CCMU_SDMMC2:
	case CCMU_SDMMC3:
		SXICLR4(sc, CCMU_AHB_GATING0,
		    CCMU_AHB_GATING_SDMMCx(mod - CCMU_SDMMC0));
		break;
	case CCMU_TWI0:
	case CCMU_TWI1:
	case CCMU_TWI2:
	case CCMU_TWI3:
	case CCMU_TWI4:
		SXICLR4(sc, CCMU_APB_GATING1, mod == CCMU_TWI4
		    ? CCMU_APB_GATING_TWI4
		    : CCMU_APB_GATING_TWIx(mod - CCMU_TWI0));
		break;
	case CCMU_PIO:
		SXICLR4(sc, CCMU_APB_GATING0, CCMU_APB_GATING_PIO);
		break;
	default:
		break;
	}
}

void
sxiccmu_set_sd_clock(int mod, int freq)
{
	struct sxiccmu_softc *sc = sxiccmu_cd.cd_devs[0];
	uint32_t clk;
	int m, n;

	if (freq <= 400000) {
		n = 2;
		if (freq > 0)
			m = ((24000000 / (1 << n)) / freq) - 1;
		else
			m = 15;
	} else {
		n = 0;
		m = 0;
	}
	
	clk = SXIREAD4(sc, CCMU_SDx_CLK(mod - CCMU_SDMMC0));
	clk &= ~CCMU_SDx_CLK_SRC_GATING_MASK;
	clk |= CCMU_SDx_CLK_SRC_GATING_OSC24M;
	clk &= ~CCMU_SDx_CLK_FACTOR_N;
	clk |= n << CCMU_SDx_CLK_FACTOR_N_SHIFT;
	clk &= ~CCMU_SDx_CLK_FACTOR_M;
	clk |= m << CCMU_SDx_CLK_FACTOR_M_SHIFT;
	clk |= CCMU_SDx_CLK_GATING;
	SXIWRITE4(sc, CCMU_SDx_CLK(mod - CCMU_SDMMC0), clk);
}
