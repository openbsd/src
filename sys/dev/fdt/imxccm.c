/* $OpenBSD: imxccm.c,v 1.5 2018/06/11 09:20:46 kettenis Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/fdt.h>

#include <dev/fdt/imxanatopvar.h>

/* registers */
#define CCM_CCR		0x00
#define CCM_CCDR	0x04
#define CCM_CSR		0x08
#define CCM_CCSR	0x0c
#define CCM_CACRR	0x10
#define CCM_CBCDR	0x14
#define CCM_CBCMR	0x18
#define CCM_CSCMR1	0x1c
#define CCM_CSCMR2	0x20
#define CCM_CSCDR1	0x24
#define CCM_CS1CDR	0x28
#define CCM_CS2CDR	0x2c
#define CCM_CDCDR	0x30
#define CCM_CHSCCDR	0x34
#define CCM_CSCDR2	0x38
#define CCM_CSCDR3	0x3c
#define CCM_CSCDR4	0x40
#define CCM_CDHIPR	0x48
#define CCM_CDCR	0x4c
#define CCM_CTOR	0x50
#define CCM_CLPCR	0x54
#define CCM_CISR	0x58
#define CCM_CIMR	0x5c
#define CCM_CCOSR	0x60
#define CCM_CGPR	0x64
#define CCM_CCGR0	0x68
#define CCM_CCGR1	0x6c
#define CCM_CCGR2	0x70
#define CCM_CCGR3	0x74
#define CCM_CCGR4	0x78
#define CCM_CCGR5	0x7c
#define CCM_CCGR6	0x80
#define CCM_CCGR7	0x84
#define CCM_CMEOR	0x88

/* bits and bytes */
#define CCM_CCSR_PLL3_SW_CLK_SEL		(1 << 0)
#define CCM_CCSR_PLL2_SW_CLK_SEL		(1 << 1)
#define CCM_CCSR_PLL1_SW_CLK_SEL		(1 << 2)
#define CCM_CCSR_STEP_SEL			(1 << 8)
#define CCM_CBCDR_IPG_PODF_SHIFT		8
#define CCM_CBCDR_IPG_PODF_MASK			0x3
#define CCM_CBCDR_AHB_PODF_SHIFT		10
#define CCM_CBCDR_AHB_PODF_MASK			0x7
#define CCM_CBCDR_PERIPH_CLK_SEL_SHIFT		25
#define CCM_CBCDR_PERIPH_CLK_SEL_MASK		0x1
#define CCM_CBCMR_PERIPH_CLK2_SEL_SHIFT		12
#define CCM_CBCMR_PERIPH_CLK2_SEL_MASK		0x3
#define CCM_CBCMR_PRE_PERIPH_CLK_SEL_SHIFT	18
#define CCM_CBCMR_PRE_PERIPH_CLK_SEL_MASK	0x3
#define CCM_CSCDR1_USDHCx_CLK_SEL_SHIFT(x)	((x) + 15)
#define CCM_CSCDR1_USDHCx_CLK_SEL_MASK		0x1
#define CCM_CSCDR1_USDHCx_PODF_MASK		0x7
#define CCM_CSCDR1_UART_PODF_MASK		0x7
#define CCM_CCGR1_ENET				(3 << 10)
#define CCM_CCGR4_125M_PCIE			(3 << 0)
#define CCM_CCGR5_100M_SATA			(3 << 4)
#define CCM_CSCMR1_PERCLK_CLK_PODF_MASK		0x1f
#define CCM_CSCMR1_PERCLK_CLK_SEL_MASK		(1 << 6)

#define HCLK_FREQ				24000000
#define PLL3_80M				80000000

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct imxccm_gate {
	uint16_t reg;
	uint8_t pos;
	uint16_t parent;
};

struct imxccm_divider {
	uint16_t reg;
	uint16_t shift;
	uint16_t mask;
	uint16_t parent;
	uint16_t fixed;
};

struct imxccm_mux {
	uint16_t reg;
	uint16_t shift;
	uint16_t mask;
};

#include "imxccm_clocks.h"

struct imxccm_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;
	uint32_t		sc_phandle;

	struct imxccm_gate	*sc_gates;
	int			sc_ngates;
	struct imxccm_divider	*sc_divs;
	int			sc_ndivs;
	struct imxccm_mux	*sc_muxs;
	int			sc_nmuxs;
	struct clock_device	sc_cd;
};

int	imxccm_match(struct device *, void *, void *);
void	imxccm_attach(struct device *parent, struct device *self, void *args);

struct cfattach	imxccm_ca = {
	sizeof (struct imxccm_softc), imxccm_match, imxccm_attach
};

struct cfdriver imxccm_cd = {
	NULL, "imxccm", DV_DULL
};

uint32_t imxccm_get_armclk(struct imxccm_softc *);
void imxccm_armclk_set_parent(struct imxccm_softc *, enum imxanatop_clocks);
uint32_t imxccm_get_usdhx(struct imxccm_softc *, int x);
uint32_t imxccm_get_periphclk(struct imxccm_softc *);
uint32_t imxccm_get_ahbclk(struct imxccm_softc *);
uint32_t imxccm_get_ipgclk(struct imxccm_softc *);
uint32_t imxccm_get_ipg_perclk(struct imxccm_softc *);
uint32_t imxccm_get_uartclk(struct imxccm_softc *);
uint32_t imxccm_imx8mq_i2c(struct imxccm_softc *sc, uint32_t);
uint32_t imxccm_imx8mq_uart(struct imxccm_softc *sc, uint32_t);
uint32_t imxccm_imx8mq_usdhc(struct imxccm_softc *sc, uint32_t);
uint32_t imxccm_imx8mq_usb(struct imxccm_softc *sc, uint32_t);
void imxccm_enable(void *, uint32_t *, int);
uint32_t imxccm_get_frequency(void *, uint32_t *);
int imxccm_set_frequency(void *, uint32_t *, uint32_t);
int imxccm_set_parent(void *, uint32_t *, uint32_t *);

int
imxccm_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "fsl,imx6q-ccm") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx6sl-ccm") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx6sx-ccm") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx6ul-ccm") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx7d-ccm") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx8mq-ccm"));
}

void
imxccm_attach(struct device *parent, struct device *self, void *aux)
{
	struct imxccm_softc *sc = (struct imxccm_softc *)self;
	struct fdt_attach_args *faa = aux;

	KASSERT(faa->fa_nreg >= 1);

	sc->sc_node = faa->fa_node;
	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	sc->sc_phandle = OF_getpropint(sc->sc_node, "phandle", 0);

	if (OF_is_compatible(sc->sc_node, "fsl,imx8mq-ccm")) {
		sc->sc_gates = imx8mq_gates;
		sc->sc_ngates = nitems(imx8mq_gates);
		sc->sc_divs = imx8mq_divs;
		sc->sc_ndivs = nitems(imx8mq_divs);
		sc->sc_muxs = imx8mq_muxs;
		sc->sc_nmuxs = nitems(imx8mq_muxs);
	} else if (OF_is_compatible(sc->sc_node, "fsl,imx7d-ccm")) {
		sc->sc_gates = imx7d_gates;
		sc->sc_ngates = nitems(imx7d_gates);
		sc->sc_divs = imx7d_divs;
		sc->sc_ndivs = nitems(imx7d_divs);
		sc->sc_muxs = imx7d_muxs;
		sc->sc_nmuxs = nitems(imx7d_muxs);
	} else if (OF_is_compatible(sc->sc_node, "fsl,imx6ul-ccm")) {
		sc->sc_gates = imx6ul_gates;
		sc->sc_ngates = nitems(imx6ul_gates);
	} else {
		sc->sc_gates = imx6_gates;
		sc->sc_ngates = nitems(imx6_gates);
	}

	printf("\n");

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	sc->sc_cd.cd_enable = imxccm_enable;
	sc->sc_cd.cd_get_frequency = imxccm_get_frequency;
	sc->sc_cd.cd_set_frequency = imxccm_set_frequency;
	sc->sc_cd.cd_set_parent = imxccm_set_parent;
	clock_register(&sc->sc_cd);
}

uint32_t
imxccm_get_armclk(struct imxccm_softc *sc)
{
	uint32_t ccsr = HREAD4(sc, CCM_CCSR);

	if (!(ccsr & CCM_CCSR_PLL1_SW_CLK_SEL))
		return imxanatop_decode_pll(ARM_PLL1, HCLK_FREQ);
	else if (ccsr & CCM_CCSR_STEP_SEL)
		return imxanatop_get_pll2_pfd(2);
	else
		return HCLK_FREQ;
}

void
imxccm_armclk_set_parent(struct imxccm_softc *sc, enum imxanatop_clocks clock)
{
	switch (clock)
	{
	case ARM_PLL1:
		/* jump onto pll1 */
		HCLR4(sc, CCM_CCSR, CCM_CCSR_PLL1_SW_CLK_SEL);
		/* put step clk on OSC, power saving */
		HCLR4(sc, CCM_CCSR, CCM_CCSR_STEP_SEL);
		break;
	case OSC:
		/* put step clk on OSC */
		HCLR4(sc, CCM_CCSR, CCM_CCSR_STEP_SEL);
		/* jump onto step clk */
		HSET4(sc, CCM_CCSR, CCM_CCSR_PLL1_SW_CLK_SEL);
		break;
	case SYS_PLL2_PFD2:
		/* put step clk on pll2-pfd2 400 MHz */
		HSET4(sc, CCM_CCSR, CCM_CCSR_STEP_SEL);
		/* jump onto step clk */
		HSET4(sc, CCM_CCSR, CCM_CCSR_PLL1_SW_CLK_SEL);
		break;
	default:
		panic("%s: parent not possible for arm clk", __func__);
	}
}

unsigned int
imxccm_get_usdhx(struct imxccm_softc *sc, int x)
{
	uint32_t cscmr1 = HREAD4(sc, CCM_CSCMR1);
	uint32_t cscdr1 = HREAD4(sc, CCM_CSCDR1);
	uint32_t podf, clkroot;

	// Odd bitsetting. Damn you.
	if (x == 1)
		podf = ((cscdr1 >> 11) & CCM_CSCDR1_USDHCx_PODF_MASK);
	else
		podf = ((cscdr1 >> (10 + 3*x)) & CCM_CSCDR1_USDHCx_PODF_MASK);

	if (cscmr1 & (1 << CCM_CSCDR1_USDHCx_CLK_SEL_SHIFT(x)))
		clkroot = imxanatop_get_pll2_pfd(0); // 352 MHz
	else
		clkroot = imxanatop_get_pll2_pfd(2); // 396 MHz

	return clkroot / (podf + 1);
}

uint32_t
imxccm_get_uartclk(struct imxccm_softc *sc)
{
	uint32_t clkroot = PLL3_80M;
	uint32_t podf = HREAD4(sc, CCM_CSCDR1) & CCM_CSCDR1_UART_PODF_MASK;

	return clkroot / (podf + 1);
}

uint32_t
imxccm_get_periphclk(struct imxccm_softc *sc)
{
	if ((HREAD4(sc, CCM_CBCDR) >> CCM_CBCDR_PERIPH_CLK_SEL_SHIFT)
		    & CCM_CBCDR_PERIPH_CLK_SEL_MASK) {
		switch((HREAD4(sc, CCM_CBCMR)
		    >> CCM_CBCMR_PERIPH_CLK2_SEL_SHIFT) & CCM_CBCMR_PERIPH_CLK2_SEL_MASK) {
		case 0:
			return imxanatop_decode_pll(USB1_PLL3, HCLK_FREQ);
		case 1:
		case 2:
			return HCLK_FREQ;
		default:
			return 0;
		}
	
	} else {
		switch((HREAD4(sc, CCM_CBCMR)
		    >> CCM_CBCMR_PRE_PERIPH_CLK_SEL_SHIFT) & CCM_CBCMR_PRE_PERIPH_CLK_SEL_MASK) {
		default:
		case 0:
			return imxanatop_decode_pll(SYS_PLL2, HCLK_FREQ);
		case 1:
			return imxanatop_get_pll2_pfd(2); // 396 MHz
		case 2:
			return imxanatop_get_pll2_pfd(0); // 352 MHz
		case 3:
			return imxanatop_get_pll2_pfd(2) / 2; // 198 MHz
		}
	}
}

uint32_t
imxccm_get_ahbclk(struct imxccm_softc *sc)
{
	uint32_t ahb_podf;

	ahb_podf = (HREAD4(sc, CCM_CBCDR) >> CCM_CBCDR_AHB_PODF_SHIFT)
	    & CCM_CBCDR_AHB_PODF_MASK;
	return imxccm_get_periphclk(sc) / (ahb_podf + 1);
}

uint32_t
imxccm_get_ipgclk(struct imxccm_softc *sc)
{
	uint32_t ipg_podf;

	ipg_podf = (HREAD4(sc, CCM_CBCDR) >> CCM_CBCDR_IPG_PODF_SHIFT)
	    & CCM_CBCDR_IPG_PODF_MASK;
	return imxccm_get_ahbclk(sc) / (ipg_podf + 1);
}

uint32_t
imxccm_get_ipg_perclk(struct imxccm_softc *sc)
{
	uint32_t cscmr1 = HREAD4(sc, CCM_CSCMR1);
	uint32_t freq, ipg_podf;

	if (sc->sc_gates == imx6ul_gates &&
	    cscmr1 & CCM_CSCMR1_PERCLK_CLK_SEL_MASK)
		freq = HCLK_FREQ;
	else
		freq = imxccm_get_ipgclk(sc);

	ipg_podf = cscmr1 & CCM_CSCMR1_PERCLK_CLK_PODF_MASK;

	return freq / (ipg_podf + 1);
}

uint32_t
imxccm_imx7d_i2c(struct imxccm_softc *sc, uint32_t idx)
{
	uint32_t mux;

	if (idx >= sc->sc_nmuxs || sc->sc_muxs[idx].reg == 0)
		return 0;

	mux = HREAD4(sc, sc->sc_muxs[idx].reg);
	mux >>= sc->sc_muxs[idx].shift;
	mux &= sc->sc_muxs[idx].mask;

	switch (mux) {
	case 0:
		return clock_get_frequency(sc->sc_node, "osc");
	case 1:
		return 120000000; /* pll_sys_main_120m_clk */
	default:
		printf("%s: 0x%08x 0x%08x\n", __func__, idx, mux);
		return 0;
	}
}

uint32_t
imxccm_imx7d_uart(struct imxccm_softc *sc, uint32_t idx)
{
	uint32_t mux;

	if (idx >= sc->sc_nmuxs || sc->sc_muxs[idx].reg == 0)
		return 0;

	mux = HREAD4(sc, sc->sc_muxs[idx].reg);
	mux >>= sc->sc_muxs[idx].shift;
	mux &= sc->sc_muxs[idx].mask;

	switch (mux) {
	case 0:
		return clock_get_frequency(sc->sc_node, "osc");
	default:
		printf("%s: 0x%08x 0x%08x\n", __func__, idx, mux);
		return 0;
	}
}

uint32_t
imxccm_imx7d_usdhc(struct imxccm_softc *sc, uint32_t idx)
{
	uint32_t mux;

	if (idx >= sc->sc_nmuxs || sc->sc_muxs[idx].reg == 0)
		return 0;

	mux = HREAD4(sc, sc->sc_muxs[idx].reg);
	mux >>= sc->sc_muxs[idx].shift;
	mux &= sc->sc_muxs[idx].mask;

	switch (mux) {
	case 0:
		return clock_get_frequency(sc->sc_node, "osc");
	case 1:
		return 392000000; /* pll_sys_pfd0_392m_clk */
	default:
		printf("%s: 0x%08x 0x%08x\n", __func__, idx, mux);
		return 0;
	}
}

uint32_t
imxccm_imx8mq_enet(struct imxccm_softc *sc, uint32_t idx)
{
	uint32_t mux;

	if (idx >= sc->sc_nmuxs || sc->sc_muxs[idx].reg == 0)
		return 0;

	mux = HREAD4(sc, sc->sc_muxs[idx].reg);
	mux >>= sc->sc_muxs[idx].shift;
	mux &= sc->sc_muxs[idx].mask;

	switch (mux) {
	case 0:
		return clock_get_frequency(sc->sc_node, "osc_25m");
	case 1:
		return 266 * 1000 * 1000; /* sys1_pll_266m */
	default:
		printf("%s: 0x%08x 0x%08x\n", __func__, idx, mux);
		return 0;
	}
}

uint32_t
imxccm_imx8mq_i2c(struct imxccm_softc *sc, uint32_t idx)
{
	uint32_t mux;

	if (idx >= sc->sc_nmuxs || sc->sc_muxs[idx].reg == 0)
		return 0;

	mux = HREAD4(sc, sc->sc_muxs[idx].reg);
	mux >>= sc->sc_muxs[idx].shift;
	mux &= sc->sc_muxs[idx].mask;

	switch (mux) {
	case 0:
		return clock_get_frequency(sc->sc_node, "osc_25m");
	default:
		printf("%s: 0x%08x 0x%08x\n", __func__, idx, mux);
		return 0;
	}
}

uint32_t
imxccm_imx8mq_uart(struct imxccm_softc *sc, uint32_t idx)
{
	uint32_t mux;

	if (idx >= sc->sc_nmuxs || sc->sc_muxs[idx].reg == 0)
		return 0;

	mux = HREAD4(sc, sc->sc_muxs[idx].reg);
	mux >>= sc->sc_muxs[idx].shift;
	mux &= sc->sc_muxs[idx].mask;

	switch (mux) {
	case 0:
		return clock_get_frequency(sc->sc_node, "osc_25m");
	default:
		printf("%s: 0x%08x 0x%08x\n", __func__, idx, mux);
		return 0;
	}
}

uint32_t
imxccm_imx8mq_usdhc(struct imxccm_softc *sc, uint32_t idx)
{
	uint32_t mux;

	if (idx >= sc->sc_nmuxs || sc->sc_muxs[idx].reg == 0)
		return 0;

	mux = HREAD4(sc, sc->sc_muxs[idx].reg);
	mux >>= sc->sc_muxs[idx].shift;
	mux &= sc->sc_muxs[idx].mask;

	switch (mux) {
	case 0:
		return clock_get_frequency(sc->sc_node, "osc_25m");
	case 1:
		return 400 * 1000 * 1000; /* sys1_pll_400m */
	default:
		printf("%s: 0x%08x 0x%08x\n", __func__, idx, mux);
		return 0;
	}
}

uint32_t
imxccm_imx8mq_usb(struct imxccm_softc *sc, uint32_t idx)
{
	uint32_t mux;

	if (idx >= sc->sc_nmuxs || sc->sc_muxs[idx].reg == 0)
		return 0;

	mux = HREAD4(sc, sc->sc_muxs[idx].reg);
	mux >>= sc->sc_muxs[idx].shift;
	mux &= sc->sc_muxs[idx].mask;

	switch (mux) {
	case 0:
		return clock_get_frequency(sc->sc_node, "osc_25m");
	case 1:
		if (idx == IMX8MQ_CLK_USB_CORE_REF_SRC ||
		    idx == IMX8MQ_CLK_USB_PHY_REF_SRC)
			return 100 * 1000 * 1000; /* sys1_pll_100m */
		if (idx == IMX8MQ_CLK_USB_BUS_SRC)
			return 500 * 1000 * 1000; /* sys2_pll_500m */
		printf("%s: 0x%08x 0x%08x\n", __func__, idx, mux);
		return 0;
	default:
		printf("%s: 0x%08x 0x%08x\n", __func__, idx, mux);
		return 0;
	}
}

void
imxccm_enable(void *cookie, uint32_t *cells, int on)
{
	struct imxccm_softc *sc = cookie;
	uint32_t idx = cells[0], parent;
	uint16_t reg;
	uint8_t pos;

	/* Dummy clock. */
	if (idx == 0)
		return;

	if (sc->sc_gates == imx6_gates) {
		switch (idx) {
		case IMX6_CLK_USBPHY1:
			imxanatop_enable_pll_usb1();
			return;
		case IMX6_CLK_USBPHY2:
			imxanatop_enable_pll_usb2();
			return;
		case IMX6_CLK_SATA_REF_100:
			imxanatop_enable_sata();
			return;
		case IMX6_CLK_ENET_REF:
			imxanatop_enable_enet();
			return;
		case IMX6_CLK_IPG:
		case IMX6_CLK_IPG_PER:
			/* always on */
			return;
		default:
			break;
		}
	}

	if (on) {
		if (idx < sc->sc_ngates && sc->sc_gates[idx].parent) {
			parent = sc->sc_gates[idx].parent;
			imxccm_enable(sc, &parent, on);
		}

		if (idx < sc->sc_ndivs && sc->sc_divs[idx].parent) {
			parent = sc->sc_divs[idx].parent;
			imxccm_enable(sc, &parent, on);
		}
	}

	if ((idx < sc->sc_ndivs && sc->sc_divs[idx].reg != 0) ||
	    (idx < sc->sc_nmuxs && sc->sc_muxs[idx].reg != 0))
		return;

	if (idx >= sc->sc_ngates || sc->sc_gates[idx].reg == 0) {
		printf("%s: 0x%08x\n", __func__, idx);
		return;
	}

	reg = sc->sc_gates[idx].reg;
	pos = sc->sc_gates[idx].pos;

	if (on)
		HSET4(sc, reg, 0x3 << (2 * pos));
	else
		HCLR4(sc, reg, 0x3 << (2 * pos));
}

uint32_t
imxccm_get_frequency(void *cookie, uint32_t *cells)
{
	struct imxccm_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t div, parent;

	/* Dummy clock. */
	if (idx == 0)
		return 0;

	if (idx < sc->sc_ngates && sc->sc_gates[idx].parent) {
		parent = sc->sc_gates[idx].parent;
		return imxccm_get_frequency(sc, &parent);
	}

	if (idx < sc->sc_ndivs && sc->sc_divs[idx].parent) {
		div = HREAD4(sc, sc->sc_divs[idx].reg);
		div = div >> sc->sc_divs[idx].shift;
		div = div & sc->sc_divs[idx].mask;
		parent = sc->sc_divs[idx].parent;
		return imxccm_get_frequency(sc, &parent) / (div + 1);
	}

	if (sc->sc_gates == imx8mq_gates) {
		switch (idx) {
		case IMX8MQ_CLK_A53_SRC:
			return 1000 * 1000 * 1000; /* arm_pll */
		case IMX8MQ_CLK_ENET_AXI_SRC:
			return imxccm_imx8mq_enet(sc, idx);
		case IMX8MQ_CLK_I2C1_SRC:
		case IMX8MQ_CLK_I2C2_SRC:
		case IMX8MQ_CLK_I2C3_SRC:
		case IMX8MQ_CLK_I2C4_SRC:
			return imxccm_imx8mq_i2c(sc, idx);
		case IMX8MQ_CLK_UART1_SRC:
		case IMX8MQ_CLK_UART2_SRC:
		case IMX8MQ_CLK_UART3_SRC:
		case IMX8MQ_CLK_UART4_SRC:
			return imxccm_imx8mq_uart(sc, idx);
		case IMX8MQ_CLK_USDHC1_SRC:
		case IMX8MQ_CLK_USDHC2_SRC:
			return imxccm_imx8mq_usdhc(sc, idx);
		case IMX8MQ_CLK_USB_BUS_SRC:
		case IMX8MQ_CLK_USB_CORE_REF_SRC:
		case IMX8MQ_CLK_USB_PHY_REF_SRC:
			return imxccm_imx8mq_usb(sc, idx);
		}
	} else if (sc->sc_gates == imx7d_gates) {
		switch (idx) {
		case IMX7D_I2C1_ROOT_SRC:
		case IMX7D_I2C2_ROOT_SRC:
		case IMX7D_I2C3_ROOT_SRC:
		case IMX7D_I2C4_ROOT_SRC:
			return imxccm_imx7d_i2c(sc, idx);
		case IMX7D_UART1_ROOT_SRC:
		case IMX7D_UART2_ROOT_SRC:
		case IMX7D_UART3_ROOT_SRC:
		case IMX7D_UART4_ROOT_SRC:
		case IMX7D_UART5_ROOT_SRC:
		case IMX7D_UART6_ROOT_SRC:
		case IMX7D_UART7_ROOT_SRC:
			return imxccm_imx7d_uart(sc, idx);
		case IMX7D_USDHC1_ROOT_SRC:
		case IMX7D_USDHC2_ROOT_SRC:
		case IMX7D_USDHC3_ROOT_SRC:
			return imxccm_imx7d_usdhc(sc, idx);
		}
	} else if (sc->sc_gates == imx6ul_gates) {
		switch (idx) {
		case IMX6UL_CLK_ARM:
			return imxccm_get_armclk(sc);
		case IMX6UL_CLK_IPG:
			return imxccm_get_ipgclk(sc);
		case IMX6UL_CLK_PERCLK:
			return imxccm_get_ipg_perclk(sc);
		case IMX6UL_CLK_UART1_SERIAL:
			return imxccm_get_uartclk(sc);
		case IMX6UL_CLK_USDHC1:
		case IMX6UL_CLK_USDHC2:
			return imxccm_get_usdhx(sc, idx - IMX6UL_CLK_USDHC1 + 1);
		}
	} else if (sc->sc_gates == imx6_gates) {
		switch (idx) {
		case IMX6_CLK_AHB:
			return imxccm_get_ahbclk(sc);
		case IMX6_CLK_ARM:
			return imxccm_get_armclk(sc);
		case IMX6_CLK_IPG:
			return imxccm_get_ipgclk(sc);
		case IMX6_CLK_IPG_PER:
			return imxccm_get_ipg_perclk(sc);
		case IMX6_CLK_UART_SERIAL:
			return imxccm_get_uartclk(sc);
		case IMX6_CLK_USDHC1:
		case IMX6_CLK_USDHC2:
		case IMX6_CLK_USDHC3:
		case IMX6_CLK_USDHC4:
			return imxccm_get_usdhx(sc, idx - IMX6_CLK_USDHC1 + 1);
		}
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
imxccm_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	struct imxccm_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t reg, div, parent, parent_freq;

	if (sc->sc_divs == imx8mq_divs) {
		switch (idx) {
		case IMX8MQ_CLK_USB_BUS_SRC:
		case IMX8MQ_CLK_USB_CORE_REF_SRC:
		case IMX8MQ_CLK_USB_PHY_REF_SRC:
			if (imxccm_get_frequency(sc, cells) != freq)
				break;
			return 0;
		case IMX8MQ_CLK_USDHC1_DIV:
			parent = sc->sc_divs[idx].parent;
			if (imxccm_get_frequency(sc, &parent) != freq)
				break;
			imxccm_enable(cookie, &parent, 1);
			reg = HREAD4(sc, sc->sc_divs[idx].reg);
			reg &= ~(sc->sc_divs[idx].mask << sc->sc_divs[idx].shift);
			reg |= (0x0 << sc->sc_divs[idx].shift);
			HWRITE4(sc, sc->sc_divs[idx].reg, reg);
			return 0;
		}
	} else if (sc->sc_divs == imx7d_divs) {
		switch (idx) {
		case IMX7D_USDHC1_ROOT_CLK:
		case IMX7D_USDHC2_ROOT_CLK:
		case IMX7D_USDHC3_ROOT_CLK:
			parent = sc->sc_gates[idx].parent;
			return imxccm_set_frequency(sc, &parent, freq);
		case IMX7D_USDHC1_ROOT_DIV:
		case IMX7D_USDHC2_ROOT_DIV:
		case IMX7D_USDHC3_ROOT_DIV:
			parent = sc->sc_divs[idx].parent;
			parent_freq = imxccm_get_frequency(sc, &parent);
			div = 0;
			while (parent_freq / (div + 1) > freq)
				div++;
			reg = HREAD4(sc, sc->sc_divs[idx].reg);
			reg &= ~(sc->sc_divs[idx].mask << sc->sc_divs[idx].shift);
			reg |= (div << sc->sc_divs[idx].shift);
			HWRITE4(sc, sc->sc_divs[idx].reg, reg);
			return 0;
		}
	}

	printf("%s: 0x%08x %x\n", __func__, idx, freq);
	return -1;
}

int
imxccm_set_parent(void *cookie, uint32_t *cells, uint32_t *pcells)
{
	struct imxccm_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t pidx;
	uint32_t mux;

	if (pcells[0] != sc->sc_phandle) {
		printf("%s: 0x%08x parent 0x%08x\n", __func__, idx, pcells[0]);
		return -1;
	}

	pidx = pcells[1];

	if (sc->sc_muxs == imx8mq_muxs) {
		switch (idx) {
		case IMX8MQ_CLK_USB_BUS_SRC:
			if (pidx != IMX8MQ_SYS2_PLL_500M)
				break;
			mux = HREAD4(sc, sc->sc_muxs[idx].reg);
			mux &= ~(sc->sc_muxs[idx].mask << sc->sc_muxs[idx].shift);
			mux |= (0x1 << sc->sc_muxs[idx].shift);
			HWRITE4(sc, sc->sc_muxs[idx].reg, mux);
			return 0;
		case IMX8MQ_CLK_USB_CORE_REF_SRC:
		case IMX8MQ_CLK_USB_PHY_REF_SRC:
			if (pidx != IMX8MQ_SYS1_PLL_100M)
				break;
			mux = HREAD4(sc, sc->sc_muxs[idx].reg);
			mux &= ~(sc->sc_muxs[idx].mask << sc->sc_muxs[idx].shift);
			mux |= (0x1 << sc->sc_muxs[idx].shift);
			HWRITE4(sc, sc->sc_muxs[idx].reg, mux);
			return 0;
		}
	}

	printf("%s: 0x%08x 0x%08x\n", __func__, idx, pidx);
	return -1;
}
