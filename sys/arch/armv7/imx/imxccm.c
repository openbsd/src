/* $OpenBSD: imxccm.c,v 1.11 2018/04/01 18:50:54 patrick Exp $ */
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

/* ANALOG */
#define CCM_ANALOG_PLL_ARM			0x4000
#define CCM_ANALOG_PLL_ARM_SET			0x4004
#define CCM_ANALOG_PLL_ARM_CLR			0x4008
#define CCM_ANALOG_PLL_USB1			0x4010
#define CCM_ANALOG_PLL_USB1_SET			0x4014
#define CCM_ANALOG_PLL_USB1_CLR			0x4018
#define CCM_ANALOG_PLL_USB2			0x4020
#define CCM_ANALOG_PLL_USB2_SET			0x4024
#define CCM_ANALOG_PLL_USB2_CLR			0x4028
#define CCM_ANALOG_PLL_SYS			0x4030
#define CCM_ANALOG_USB1_CHRG_DETECT		0x41b0
#define CCM_ANALOG_USB1_CHRG_DETECT_SET		0x41b4
#define CCM_ANALOG_USB1_CHRG_DETECT_CLR		0x41b8
#define CCM_ANALOG_USB2_CHRG_DETECT		0x4210
#define CCM_ANALOG_USB2_CHRG_DETECT_SET		0x4214
#define CCM_ANALOG_USB2_CHRG_DETECT_CLR		0x4218
#define CCM_ANALOG_DIGPROG			0x4260
#define CCM_ANALOG_PLL_ENET			0x40e0
#define CCM_ANALOG_PLL_ENET_SET			0x40e4
#define CCM_ANALOG_PLL_ENET_CLR			0x40e8
#define CCM_ANALOG_PFD_480			0x40f0
#define CCM_ANALOG_PFD_480_SET			0x40f4
#define CCM_ANALOG_PFD_480_CLR			0x40f8
#define CCM_ANALOG_PFD_528			0x4100
#define CCM_ANALOG_PFD_528_SET			0x4104
#define CCM_ANALOG_PFD_528_CLR			0x4108
#define CCM_PMU_MISC1				0x4160

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
#define CCM_ANALOG_PLL_ARM_DIV_SELECT_MASK	0x7f
#define CCM_ANALOG_PLL_ARM_BYPASS		(1 << 16)
#define CCM_ANALOG_PLL_USB1_DIV_SELECT_MASK	0x1
#define CCM_ANALOG_PLL_USB1_EN_USB_CLKS		(1 << 6)
#define CCM_ANALOG_PLL_USB1_POWER		(1 << 12)
#define CCM_ANALOG_PLL_USB1_ENABLE		(1 << 13)
#define CCM_ANALOG_PLL_USB1_BYPASS		(1 << 16)
#define CCM_ANALOG_PLL_USB1_LOCK		(1 << 31)
#define CCM_ANALOG_PLL_USB2_DIV_SELECT_MASK	0x1
#define CCM_ANALOG_PLL_USB2_EN_USB_CLKS		(1 << 6)
#define CCM_ANALOG_PLL_USB2_POWER		(1 << 12)
#define CCM_ANALOG_PLL_USB2_ENABLE		(1 << 13)
#define CCM_ANALOG_PLL_USB2_BYPASS		(1 << 16)
#define CCM_ANALOG_PLL_USB2_LOCK		(1U << 31)
#define CCM_ANALOG_PLL_SYS_DIV_SELECT_MASK	0x1
#define CCM_ANALOG_USB1_CHRG_DETECT_CHK_CHRG_B	(1 << 19)
#define CCM_ANALOG_USB1_CHRG_DETECT_EN_B	(1 << 20)
#define CCM_ANALOG_USB2_CHRG_DETECT_CHK_CHRG_B	(1 << 19)
#define CCM_ANALOG_USB2_CHRG_DETECT_EN_B	(1 << 20)
#define CCM_ANALOG_DIGPROG_MINOR_MASK		0xff
#define CCM_ANALOG_PLL_ENET_DIV_125M		(1 << 11)
#define CCM_ANALOG_PLL_ENET_POWERDOWN		(1 << 12)
#define CCM_ANALOG_PLL_ENET_ENABLE		(1 << 13)
#define CCM_ANALOG_PLL_ENET_BYPASS		(1 << 16)
#define CCM_ANALOG_PLL_ENET_125M_PCIE		(1 << 19)
#define CCM_ANALOG_PLL_ENET_100M_SATA		(1 << 20)
#define CCM_ANALOG_PLL_ENET_LOCK		(1U << 31)
#define CCM_ANALOG_PFD_480_PFDx_FRAC(x, y)	(((x) >> ((y) << 3)) & 0x3f)
#define CCM_ANALOG_PFD_528_PFDx_FRAC(x, y)	(((x) >> ((y) << 3)) & 0x3f)
#define CCM_PMU_MISC1_LVDSCLK1_CLK_SEL_SATA	(0xB << 0)
#define CCM_PMU_MISC1_LVDSCLK1_CLK_SEL_MASK	(0x1f << 0)
#define CCM_PMU_MISC1_LVDSCLK1_OBEN		(1 << 10)
#define CCM_PMU_MISC1_LVDSCLK1_IBEN		(1 << 12)

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

enum clocks {
	/* OSC */
	OSC,		/* 24 MHz OSC */

	/* PLLs */
	ARM_PLL1,	/* ARM core PLL */
	SYS_PLL2,	/* System PLL: 528 MHz */
	USB1_PLL3,	/* OTG USB PLL: 480 MHz */
	USB2_PLL,	/* Host USB PLL: 480 MHz */
	AUD_PLL4,	/* Audio PLL */
	VID_PLL5,	/* Video PLL */
	ENET_PLL6,	/* ENET PLL */
	MLB_PLL,	/* MLB PLL */

	/* SYS_PLL2 PFDs */
	SYS_PLL2_PFD0,	/* 352 MHz */
	SYS_PLL2_PFD1,	/* 594 MHz */
	SYS_PLL2_PFD2,	/* 396 MHz */

	/* USB1_PLL3 PFDs */
	USB1_PLL3_PFD0,	/* 720 MHz */
	USB1_PLL3_PFD1,	/* 540 MHz */
	USB1_PLL3_PFD2,	/* 508.2 MHz */
	USB1_PLL3_PFD3,	/* 454.7 MHz */
};

struct imxccm_softc *imxccm_sc;

int	imxccm_match(struct device *, void *, void *);
void	imxccm_attach(struct device *parent, struct device *self, void *args);

struct cfattach	imxccm_ca = {
	sizeof (struct imxccm_softc), imxccm_match, imxccm_attach
};

struct cfdriver imxccm_cd = {
	NULL, "imxccm", DV_DULL
};

uint32_t imxccm_decode_pll(enum clocks, uint32_t);
uint32_t imxccm_get_pll2_pfd(unsigned int);
uint32_t imxccm_get_pll3_pfd(unsigned int);
uint32_t imxccm_get_armclk(void);
void imxccm_armclk_set_parent(enum clocks);
uint32_t imxccm_get_usdhx(int x);
uint32_t imxccm_get_periphclk(void);
uint32_t imxccm_get_ahbclk(void);
uint32_t imxccm_get_ipgclk(void);
uint32_t imxccm_get_ipg_perclk(void);
uint32_t imxccm_get_uartclk(void);
void imxccm_enable(void *, uint32_t *, int);
uint32_t imxccm_get_frequency(void *, uint32_t *);
void imxccm_disable_usb1_chrg_detect(void);
void imxccm_disable_usb2_chrg_detect(void);
void imxccm_enable_pll_usb1(void);
void imxccm_enable_pll_usb2(void);
void imxccm_enable_pll_enet(void);
void imxccm_enable_enet(void);
void imxccm_enable_sata(void);
void imxccm_enable_pcie(void);

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

	imxccm_sc = sc;
	sc->sc_node = faa->fa_node;
	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size + 0x1000, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	if (OF_is_compatible(sc->sc_node, "fsl,imx6ul-ccm")) {
		sc->sc_gates = imx6ul_gates;
		sc->sc_ngates = nitems(imx6ul_gates);
	} else {
		sc->sc_gates = imx6_gates;
		sc->sc_ngates = nitems(imx6_gates);
	}

	printf(": imx6 rev 1.%d CPU freq: %d MHz",
	    HREAD4(sc, CCM_ANALOG_DIGPROG) & CCM_ANALOG_DIGPROG_MINOR_MASK,
	    imxccm_get_armclk() / 1000000);

	printf("\n");

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	sc->sc_cd.cd_enable = imxccm_enable;
	sc->sc_cd.cd_get_frequency = imxccm_get_frequency;
	clock_register(&sc->sc_cd);
}

uint32_t
imxccm_decode_pll(enum clocks pll, uint32_t freq)
{
	struct imxccm_softc *sc = imxccm_sc;
	uint32_t div;

	switch (pll) {
	case ARM_PLL1:
		if (HREAD4(sc, CCM_ANALOG_PLL_ARM)
		    & CCM_ANALOG_PLL_ARM_BYPASS)
			return freq;
		div = HREAD4(sc, CCM_ANALOG_PLL_ARM)
		    & CCM_ANALOG_PLL_ARM_DIV_SELECT_MASK;
		return (freq * div) / 2;
	case SYS_PLL2:
		div = HREAD4(sc, CCM_ANALOG_PLL_SYS)
		    & CCM_ANALOG_PLL_SYS_DIV_SELECT_MASK;
		return freq * (20 + (div << 1));
	case USB1_PLL3:
		div = HREAD4(sc, CCM_ANALOG_PLL_USB2)
		    & CCM_ANALOG_PLL_USB2_DIV_SELECT_MASK;
		return freq * (20 + (div << 1));
	default:
		return 0;
	}
}

uint32_t
imxccm_get_pll2_pfd(unsigned int pfd)
{
	struct imxccm_softc *sc = imxccm_sc;

	return imxccm_decode_pll(SYS_PLL2, HCLK_FREQ) * 18ULL
	    / CCM_ANALOG_PFD_528_PFDx_FRAC(HREAD4(sc, CCM_ANALOG_PFD_528), pfd);
}

uint32_t
imxccm_get_pll3_pfd(unsigned int pfd)
{
	struct imxccm_softc *sc = imxccm_sc;

	return imxccm_decode_pll(USB1_PLL3, HCLK_FREQ) * 18ULL
	    / CCM_ANALOG_PFD_480_PFDx_FRAC(HREAD4(sc, CCM_ANALOG_PFD_480), pfd);
}

uint32_t
imxccm_get_armclk(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	uint32_t ccsr = HREAD4(sc, CCM_CCSR);

	if (!(ccsr & CCM_CCSR_PLL1_SW_CLK_SEL))
		return imxccm_decode_pll(ARM_PLL1, HCLK_FREQ);
	else if (ccsr & CCM_CCSR_STEP_SEL)
		return imxccm_get_pll2_pfd(2);
	else
		return HCLK_FREQ;
}

void
imxccm_armclk_set_parent(enum clocks clock)
{
	struct imxccm_softc *sc = imxccm_sc;

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
imxccm_get_usdhx(int x)
{
	struct imxccm_softc *sc = imxccm_sc;
	uint32_t cscmr1 = HREAD4(sc, CCM_CSCMR1);
	uint32_t cscdr1 = HREAD4(sc, CCM_CSCDR1);
	uint32_t podf, clkroot;

	// Odd bitsetting. Damn you.
	if (x == 1)
		podf = ((cscdr1 >> 11) & CCM_CSCDR1_USDHCx_PODF_MASK);
	else
		podf = ((cscdr1 >> (10 + 3*x)) & CCM_CSCDR1_USDHCx_PODF_MASK);

	if (cscmr1 & (1 << CCM_CSCDR1_USDHCx_CLK_SEL_SHIFT(x)))
		clkroot = imxccm_get_pll2_pfd(0); // 352 MHz
	else
		clkroot = imxccm_get_pll2_pfd(2); // 396 MHz

	return clkroot / (podf + 1);
}

uint32_t
imxccm_get_uartclk(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	uint32_t clkroot = PLL3_80M;
	uint32_t podf = HREAD4(sc, CCM_CSCDR1) & CCM_CSCDR1_UART_PODF_MASK;

	return clkroot / (podf + 1);
}

uint32_t
imxccm_get_periphclk(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	if ((HREAD4(sc, CCM_CBCDR) >> CCM_CBCDR_PERIPH_CLK_SEL_SHIFT)
		    & CCM_CBCDR_PERIPH_CLK_SEL_MASK) {
		switch((HREAD4(sc, CCM_CBCMR)
		    >> CCM_CBCMR_PERIPH_CLK2_SEL_SHIFT) & CCM_CBCMR_PERIPH_CLK2_SEL_MASK) {
		case 0:
			return imxccm_decode_pll(USB1_PLL3, HCLK_FREQ);
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
			return imxccm_decode_pll(SYS_PLL2, HCLK_FREQ);
		case 1:
			return imxccm_get_pll2_pfd(2); // 396 MHz
		case 2:
			return imxccm_get_pll2_pfd(0); // 352 MHz
		case 3:
			return imxccm_get_pll2_pfd(2) / 2; // 198 MHz
		}
	}
}

uint32_t
imxccm_get_ahbclk(void)
{
	struct imxccm_softc *sc = imxccm_sc;
	uint32_t ahb_podf;

	ahb_podf = (HREAD4(sc, CCM_CBCDR) >> CCM_CBCDR_AHB_PODF_SHIFT)
	    & CCM_CBCDR_AHB_PODF_MASK;
	return imxccm_get_periphclk() / (ahb_podf + 1);
}

uint32_t
imxccm_get_ipgclk(void)
{
	struct imxccm_softc *sc = imxccm_sc;
	uint32_t ipg_podf;

	ipg_podf = (HREAD4(sc, CCM_CBCDR) >> CCM_CBCDR_IPG_PODF_SHIFT)
	    & CCM_CBCDR_IPG_PODF_MASK;
	return imxccm_get_ahbclk() / (ipg_podf + 1);
}

uint32_t
imxccm_get_ipg_perclk(void)
{
	struct imxccm_softc *sc = imxccm_sc;
	uint32_t cscmr1 = HREAD4(sc, CCM_CSCMR1);
	uint32_t freq, ipg_podf;

	if (sc->sc_gates == imx6ul_gates &&
	    cscmr1 & CCM_CSCMR1_PERCLK_CLK_SEL_MASK)
		freq = HCLK_FREQ;
	else
		freq = imxccm_get_ipgclk();

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
			return imxccm_get_armclk();
		case IMX6UL_CLK_IPG:
			return imxccm_get_ipgclk();
		case IMX6UL_CLK_PERCLK:
			return imxccm_get_ipg_perclk();
		case IMX6UL_CLK_UART1_SERIAL:
			return imxccm_get_uartclk();
		case IMX6UL_CLK_USDHC1:
		case IMX6UL_CLK_USDHC2:
			return imxccm_get_usdhx(idx - IMX6UL_CLK_USDHC1 + 1);
		}
	} else {
		switch (idx) {
		case IMX6_CLK_ARM:
			return imxccm_get_armclk();
		case IMX6_CLK_IPG:
			return imxccm_get_ipgclk();
		case IMX6_CLK_IPG_PER:
			return imxccm_get_ipg_perclk();
		case IMX6_CLK_UART_SERIAL:
			return imxccm_get_uartclk();
		case IMX6_CLK_USDHC1:
		case IMX6_CLK_USDHC2:
		case IMX6_CLK_USDHC3:
		case IMX6_CLK_USDHC4:
			return imxccm_get_usdhx(idx - IMX6_CLK_USDHC1 + 1);
		}
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

void
imxccm_enable_pll_enet(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	if (HREAD4(sc, CCM_ANALOG_PLL_ENET) & CCM_ANALOG_PLL_ENET_ENABLE)
		return;

	HCLR4(sc, CCM_ANALOG_PLL_ENET, CCM_ANALOG_PLL_ENET_POWERDOWN);

	HSET4(sc, CCM_ANALOG_PLL_ENET, CCM_ANALOG_PLL_ENET_ENABLE);

	while(!(HREAD4(sc, CCM_ANALOG_PLL_ENET) & CCM_ANALOG_PLL_ENET_LOCK));

	HCLR4(sc, CCM_ANALOG_PLL_ENET, CCM_ANALOG_PLL_ENET_BYPASS);
}

void
imxccm_enable_enet(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	imxccm_enable_pll_enet();
	HWRITE4(sc, CCM_ANALOG_PLL_ENET_SET, CCM_ANALOG_PLL_ENET_DIV_125M);

	HSET4(sc, CCM_CCGR1, CCM_CCGR1_ENET);
}

void
imxccm_enable_sata(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	imxccm_enable_pll_enet();
	HWRITE4(sc, CCM_ANALOG_PLL_ENET_SET, CCM_ANALOG_PLL_ENET_100M_SATA);

	HSET4(sc, CCM_CCGR5, CCM_CCGR5_100M_SATA);
}

void
imxccm_enable_pcie(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	HWRITE4(sc, CCM_PMU_MISC1,
	    (HREAD4(sc, CCM_PMU_MISC1) & ~CCM_PMU_MISC1_LVDSCLK1_CLK_SEL_MASK)
	    | CCM_PMU_MISC1_LVDSCLK1_CLK_SEL_SATA
	    | CCM_PMU_MISC1_LVDSCLK1_OBEN
	    | CCM_PMU_MISC1_LVDSCLK1_IBEN);

	imxccm_enable_pll_enet();
	HWRITE4(sc, CCM_ANALOG_PLL_ENET_SET, CCM_ANALOG_PLL_ENET_125M_PCIE);

	HSET4(sc, CCM_CCGR4, CCM_CCGR4_125M_PCIE);
}

void 
imxccm_disable_usb1_chrg_detect(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	HWRITE4(sc, CCM_ANALOG_USB1_CHRG_DETECT_SET,
	      CCM_ANALOG_USB1_CHRG_DETECT_CHK_CHRG_B
	    | CCM_ANALOG_USB1_CHRG_DETECT_EN_B);
}

void
imxccm_disable_usb2_chrg_detect(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	HWRITE4(sc, CCM_ANALOG_USB2_CHRG_DETECT_SET,
	      CCM_ANALOG_USB2_CHRG_DETECT_CHK_CHRG_B
	    | CCM_ANALOG_USB2_CHRG_DETECT_EN_B);
}

void
imxccm_enable_pll_usb1(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	HWRITE4(sc, CCM_ANALOG_PLL_USB1_CLR, CCM_ANALOG_PLL_USB1_BYPASS);

	HWRITE4(sc, CCM_ANALOG_PLL_USB1_SET,
	      CCM_ANALOG_PLL_USB1_ENABLE
	    | CCM_ANALOG_PLL_USB1_POWER
	    | CCM_ANALOG_PLL_USB1_EN_USB_CLKS);
}

void
imxccm_enable_pll_usb2(void)
{
	struct imxccm_softc *sc = imxccm_sc;

	HWRITE4(sc, CCM_ANALOG_PLL_USB2_CLR, CCM_ANALOG_PLL_USB2_BYPASS);

	HWRITE4(sc, CCM_ANALOG_PLL_USB2_SET,
	      CCM_ANALOG_PLL_USB2_ENABLE
	    | CCM_ANALOG_PLL_USB2_POWER
	    | CCM_ANALOG_PLL_USB2_EN_USB_CLKS);
}
