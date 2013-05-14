/* $OpenBSD: prcm.c,v 1.11 2013/05/14 12:01:17 rapha Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
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
 * Driver for the Power, Reset and Clock Management Module (PRCM).
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/cpufunc.h>
#include <beagle/dev/omapvar.h>
#include <beagle/dev/prcmvar.h>

#include <beagle/dev/am335x_prcmreg.h>
#include <beagle/dev/omap3_prcmreg.h>

#define PRCM_REVISION		0x0800
#define PRCM_SYSCONFIG		0x0810

uint32_t prcm_imask_mask[PRCM_REG_MAX];
uint32_t prcm_fmask_mask[PRCM_REG_MAX];
uint32_t prcm_imask_addr[PRCM_REG_MAX];
uint32_t prcm_fmask_addr[PRCM_REG_MAX];

#define SYS_CLK		13 /* SYS_CLK speed in MHz */

struct prcm_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void (*sc_enablemodule)(struct prcm_softc *sc, int mod);
	void (*sc_setclock)(struct prcm_softc *sc,
	    int clock, int speed);
};

int	prcm_match(struct device *, void *, void *);
void	prcm_attach(struct device *, struct device *, void *);
int	prcm_setup_dpll5(struct prcm_softc *);
uint32_t prcm_v3_bit(int mod);
uint32_t prcm_am335x_clkctrl(int mod);

void prcm_am335x_enablemodule(struct prcm_softc *, int);
void prcm_am335x_setclock(struct prcm_softc *, int, int);

void prcm_v3_setup(struct prcm_softc *);
void prcm_v3_enablemodule(struct prcm_softc *, int);
void prcm_v3_setclock(struct prcm_softc *, int, int);

struct cfattach	prcm_ca = {
	sizeof (struct prcm_softc), NULL, prcm_attach
};

struct cfdriver prcm_cd = {
	NULL, "prcm", DV_DULL
};

void
prcm_attach(struct device *parent, struct device *self, void *args)
{
	struct omap_attach_args *oa = args;
	struct prcm_softc *sc = (struct prcm_softc *) self;
	u_int32_t reg;

	sc->sc_iot = oa->oa_iot;

	if (bus_space_map(sc->sc_iot, oa->oa_dev->mem[0].addr,
	    oa->oa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("prcm_attach: bus_space_map failed!");

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, PRCM_REVISION);
	printf(" rev %d.%d\n", reg >> 4 & 0xf, reg & 0xf);

	switch (board_id) {
	case BOARD_ID_AM335X_BEAGLEBONE:
		sc->sc_enablemodule = prcm_am335x_enablemodule;
		sc->sc_setclock = prcm_am335x_setclock;
		break;
	case BOARD_ID_OMAP3_BEAGLE:
	case BOARD_ID_OMAP3_OVERO:
		sc->sc_enablemodule = prcm_v3_enablemodule;
		sc->sc_setclock = prcm_v3_setclock;
		prcm_v3_setup(sc);
		break;
	case BOARD_ID_OMAP4_PANDA:
		sc->sc_enablemodule = NULL;
		sc->sc_setclock = NULL;
		break;
	}
}

void
prcm_v3_setup(struct prcm_softc *sc)
{
	/* Setup the 120MHZ DPLL5 clock, to be used by USB. */
	prcm_setup_dpll5(sc);

	prcm_fmask_mask[PRCM_REG_CORE_CLK1] = PRCM_REG_CORE_CLK1_FMASK;
	prcm_imask_mask[PRCM_REG_CORE_CLK1] = PRCM_REG_CORE_CLK1_IMASK;
	prcm_fmask_addr[PRCM_REG_CORE_CLK1] = PRCM_REG_CORE_CLK1_FADDR;
	prcm_imask_addr[PRCM_REG_CORE_CLK1] = PRCM_REG_CORE_CLK1_IADDR;

	prcm_fmask_mask[PRCM_REG_CORE_CLK2] = PRCM_REG_CORE_CLK2_FMASK;
	prcm_imask_mask[PRCM_REG_CORE_CLK2] = PRCM_REG_CORE_CLK2_IMASK;
	prcm_fmask_addr[PRCM_REG_CORE_CLK2] = PRCM_REG_CORE_CLK2_FADDR;
	prcm_imask_addr[PRCM_REG_CORE_CLK2] = PRCM_REG_CORE_CLK2_IADDR;

	prcm_fmask_mask[PRCM_REG_CORE_CLK3] = PRCM_REG_CORE_CLK3_FMASK;
	prcm_imask_mask[PRCM_REG_CORE_CLK3] = PRCM_REG_CORE_CLK3_IMASK;
	prcm_fmask_addr[PRCM_REG_CORE_CLK3] = PRCM_REG_CORE_CLK3_FADDR;
	prcm_imask_addr[PRCM_REG_CORE_CLK3] = PRCM_REG_CORE_CLK3_IADDR;

	prcm_fmask_mask[PRCM_REG_USBHOST] = PRCM_REG_USBHOST_FMASK;
	prcm_imask_mask[PRCM_REG_USBHOST] = PRCM_REG_USBHOST_IMASK;
	prcm_fmask_addr[PRCM_REG_USBHOST] = PRCM_REG_USBHOST_FADDR;
	prcm_imask_addr[PRCM_REG_USBHOST] = PRCM_REG_USBHOST_IADDR;
}

void
prcm_setclock(int clock, int speed)
{
	struct prcm_softc *sc = prcm_cd.cd_devs[0];

	if (!sc->sc_setclock)
		panic("%s: not initialised!", __func__);

	sc->sc_setclock(sc, clock, speed);
}

void
prcm_am335x_setclock(struct prcm_softc *sc, int clock, int speed)
{
	u_int32_t oreg, reg, mask;

	/* set CLKSEL register */
	if (clock == 1) {
		oreg = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    PRCM_AM335X_CLKSEL_TIMER2_CLK);
		mask = 3;
		reg = oreg & ~mask;
		reg |=0x02;
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    PRCM_AM335X_CLKSEL_TIMER2_CLK, reg);
	} else if (clock == 2) {
		oreg = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    PRCM_AM335X_CLKSEL_TIMER3_CLK);
		mask = 3;
		reg = oreg & ~mask;
		reg |=0x02;
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    PRCM_AM335X_CLKSEL_TIMER3_CLK, reg);
	}
}

void
prcm_v3_setclock(struct prcm_softc *sc, int clock, int speed)
{
	u_int32_t oreg, reg, mask;

	if (clock == 1) {
		oreg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, CM_CLKSEL_WKUP);
		mask = 1;
		reg = (oreg &~mask) | (speed & mask);
		printf("%s: old %08x new %08x", __func__, oreg, reg);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, CM_CLKSEL_WKUP, reg);
	} else if (clock >= 2 && clock <= 9) {
		int shift =  (clock-2);
		oreg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, CM_CLKSEL_PER);
		mask = 1 << (shift);
		reg =  (oreg & ~mask) | ( (speed << shift) & mask);
		printf("%s: old %08x new %08x", __func__, oreg, reg);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, CM_CLKSEL_PER, reg);
	} else
		panic("%s: invalid clock %d", __func__, clock);
}

uint32_t
prcm_v3_bit(int mod)
{
	switch(mod) {
	case PRCM_MMC:
		return PRCM_CLK_EN_MMC1;
	case PRCM_USB:
		return PRCM_CLK_EN_USB;
	default:
		panic("%s: module not found\n", __func__);
	}
}

uint32_t
prcm_am335x_clkctrl(int mod)
{
	switch(mod) {
	case PRCM_TIMER2:
		return PRCM_AM335X_TIMER2_CLKCTRL;
	case PRCM_TIMER3:
		return PRCM_AM335X_TIMER3_CLKCTRL;
	case PRCM_MMC:
		return PRCM_AM335X_MMC0_CLKCTRL;
	case PRCM_USB:
		return PRCM_AM335X_USB0_CLKCTRL;
	default:
		panic("%s: module not found\n", __func__);
	}
}

void
prcm_enablemodule(int mod)
{
	struct prcm_softc *sc = prcm_cd.cd_devs[0];

	if (!sc->sc_enablemodule)
		panic("%s: not initialised!", __func__);

	sc->sc_enablemodule(sc, mod);
}

void
prcm_am335x_enablemodule(struct prcm_softc *sc, int mod)
{
	uint32_t clkctrl;
	int reg;

	/*set enable bits in CLKCTRL register */
	reg = prcm_am335x_clkctrl(mod);
	clkctrl = bus_space_read_4(sc->sc_iot, sc->sc_ioh, reg);
	clkctrl &=~AM335X_CLKCTRL_MODULEMODE_MASK;
	clkctrl |= AM335X_CLKCTRL_MODULEMODE_ENABLE;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, reg, clkctrl);

	/* wait until module is enabled */
	while (bus_space_read_4(sc->sc_iot, sc->sc_ioh, reg) & 0x30000)
		;
}

void
prcm_v3_enablemodule(struct prcm_softc *sc, int mod)
{
	uint32_t bit;
	uint32_t fclk, iclk, fmask, imask, mbit;
	int freg, ireg, reg;

	bit = prcm_v3_bit(mod);
	reg = bit >> 5;

	freg = prcm_fmask_addr[reg];
	ireg = prcm_imask_addr[reg];
	fmask = prcm_fmask_mask[reg];
	imask = prcm_imask_mask[reg];

	mbit = 1 << (bit & 0x1f);
	if (fmask & mbit) { /* dont access the register if bit isn't present */
		fclk = bus_space_read_4(sc->sc_iot, sc->sc_ioh, freg);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, freg, fclk | mbit);
	}
	if (imask & mbit) { /* dont access the register if bit isn't present */
		iclk = bus_space_read_4(sc->sc_iot, sc->sc_ioh, ireg);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, ireg, iclk | mbit);
	}
	printf("\n");
}

/*
 * OMAP35xx Power, Reset, and Clock Management Reference Guide
 * (sprufa5.pdf) and AM/DM37x Multimedia Device Technical Reference
 * Manual (sprugn4h.pdf) note that DPLL5 provides a 120MHz clock for
 * peripheral domain modules (page 107 and page 302).
 * The reference clock for DPLL5 is DPLL5_ALWON_FCLK which is
 * SYS_CLK, running at 13MHz.
 */
int
prcm_setup_dpll5(struct prcm_softc *sc)
{
	uint32_t val;

	/*
	 * We need to set the multiplier and divider values for PLL.
	 * To end up with 120MHz we take SYS_CLK, divide by it and multiply
	 * with 120 (sprugn4h.pdf, 13.4.11.4.1 SSC Configuration)
	 */
	val = ((120 & 0x7ff) << 8) | ((SYS_CLK - 1) & 0x7f);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CM_CLKSEL4_PLL, val);

	/* Clock divider from the PLL to the 120MHz clock. */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CM_CLKSEL5_PLL, val);

	/*
	 * spruf98o.pdf, page 2319:
	 * PERIPH2_DPLL_FREQSEL is 0x7 1.75MHz to 2.1MHz
	 * EN_PERIPH2_DPLL is 0x7
	 */
	val = (7 << 4) | (7 << 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CM_CLKEN2_PLL, val);

	/* Disable the interconnect clock auto-idle. */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CM_AUTOIDLE2_PLL, 0x0);

	/* Wait until DPLL5 is locked and there's clock activity. */
	while ((val = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    CM_IDLEST_CKGEN) & 0x01) == 0x00) {
#ifdef DIAGNOSTIC
		printf("CM_IDLEST_PLL = 0x%08x\n", val);
#endif
	}

	return 0;
}
