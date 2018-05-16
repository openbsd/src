/* $OpenBSD: imxccm.c,v 1.2 2018/05/16 13:42:35 patrick Exp $ */
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
	uint8_t reg;
	uint8_t pos;
	uint8_t parent;
};

#include "imxccm_clocks.h"

struct imxccm_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;

	struct imxccm_gate	*sc_gates;
	int			sc_ngates;
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
void imxccm_enable(void *, uint32_t *, int);
uint32_t imxccm_get_frequency(void *, uint32_t *);

int
imxccm_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "fsl,imx6q-ccm") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx6sl-ccm") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx6sx-ccm") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx6ul-ccm"));
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

	if (OF_is_compatible(sc->sc_node, "fsl,imx6ul-ccm")) {
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

void
imxccm_enable(void *cookie, uint32_t *cells, int on)
{
	struct imxccm_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint8_t reg, pos;

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
		default:
			break;
		}
	}

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
	uint32_t parent;

	/* Dummy clock. */
	if (idx == 0)
		return 0;

	if (idx < sc->sc_ngates && sc->sc_gates[idx].parent) {
		parent = sc->sc_gates[idx].parent;
		return imxccm_get_frequency(sc, &parent);
	}

	if (sc->sc_gates == imx6ul_gates) {
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
	} else {
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

