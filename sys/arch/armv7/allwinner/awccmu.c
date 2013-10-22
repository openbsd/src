/*	$OpenBSD: awccmu.c,v 1.1 2013/10/22 13:22:19 jasper Exp $	*/
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
#include <sys/time.h>
#include <sys/device.h>

#include <arm/cpufunc.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <armv7/allwinner/allwinnervar.h>
#include <armv7/allwinner/awccmuvar.h>

#ifdef DEBUG_CCMU
#define DPRINTF(x)	do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

#define	CCMU_SCLK_GATING		(1 << 31)
#define	CCMU_GET_CLK_DIV_RATIO_N(x)	(((x) >> 16) & 0x03)
#define	CCMU_GET_CLK_DIV_RATIO_M(x)	((x) & 0x07)

#define	CCMU_PLL6_CFG			0x28
#define	CCMU_PLL6_EN			(1 << 31)
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
#define	CCMU_AHB_GATING_SDMMC0		(1 << 8)
#define	CCMU_AHB_GATING_SDMMC1		(1 << 9)
#define	CCMU_AHB_GATING_SDMMC2		(1 << 10)
#define	CCMU_AHB_GATING_SDMMC3		(1 << 11)
#define	CCMU_AHB_GATING_NAND		(1 << 13)
#define	CCMU_AHB_GATING_EMAC		(1 << 17)
#define	CCMU_AHB_GATING_SATA		(1 << 25)

#define	CCMU_APB_GATING0		0x68
#define	CCMU_APB_GATING_PIO		(1 << 5)
#define	CCMU_APB_GATING1		0x6c
#define	CCMU_APB_GATING_UARTx(x)	(1 << (16 + (x)))
#define	CCMU_APB_GATING_TWIx(x)		(1 << (x))

#define	CCMU_NAND_CLK			0x80
#define	CCMU_NAND_CLK_SRC_GATING_OSC24M	(0 << 24)
#define	CCMU_NAND_CLK_SRC_GATING_PLL6	(1 << 24)
#define	CCMU_NAND_CLK_SRC_GATING_PLL5	(2 << 24)
#define	CCMU_NAND_CLK_SRC_GATING_MASK	(3 << 24)

#define	CCMU_SATA_CLK			0xc8
#define	CCMU_SATA_CLK_SRC_GATING	(1 << 24)

#define	CCMU_USB_CLK			0xcc
#define	CCMU_USB_PHY			(1 << 8)
#define	CCMU_SCLK_GATING_OHCI1		(1 << 7)
#define	CCMU_SCLK_GATING_OHCI0		(1 << 6)
#define	CCMU_USB2_RESET			(1 << 2)
#define	CCMU_USB1_RESET			(1 << 1)
#define	CCMU_USB0_RESET			(1 << 0)

struct awccmu_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

void	awccmu_attach(struct device *, struct device *, void *);
void	awccmu_enablemodule(int);

struct cfattach	awccmu_ca = {
	sizeof (struct awccmu_softc), NULL, awccmu_attach
};

struct cfdriver awccmu_cd = {
	NULL, "awccmu", DV_DULL
};

void
awccmu_attach(struct device *parent, struct device *self, void *args)
{
	struct awccmu_softc *sc = (struct awccmu_softc *)self;
	struct aw_attach_args *aw = args;

	sc->sc_iot = aw->aw_iot;

	if (bus_space_map(sc->sc_iot, aw->aw_dev->mem[0].addr,
	    aw->aw_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("awccmu_attach: bus_space_map failed!");

	printf("\n");
}

/* XXX spl? */
void
awccmu_enablemodule(int mod)
{
	struct awccmu_softc *sc = awccmu_cd.cd_devs[0];
	uint32_t reg;

	DPRINTF(("\nawccmu_enablemodule: mod %d\n", mod));

	switch (mod) {
	case CCMU_EHCI0:
		AWSET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_EHCI0);
		AWSET4(sc, CCMU_USB_CLK, CCMU_USB1_RESET | CCMU_USB_PHY);
		break;
	case CCMU_EHCI1:
		AWSET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_EHCI1);
		AWSET4(sc, CCMU_USB_CLK, CCMU_USB2_RESET | CCMU_USB_PHY);
		break;
	case CCMU_OHCI:
		panic("awccmu_enablemodule: XXX OHCI!");
		break;
	case CCMU_AHCI:
		reg = AWREAD4(sc, CCMU_PLL6_CFG);
		reg &= ~(CCMU_PLL6_BYPASS_EN | CCMU_PLL6_FACTOR_M |
		    CCMU_PLL6_FACTOR_N);
		reg |= CCMU_PLL6_EN | CCMU_PLL6_SATA_CLK_EN;
		reg |= 25 << 8;
		reg |= (reg >> 4 & 3);
		AWWRITE4(sc, CCMU_PLL6_CFG, reg);

		AWSET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_SATA);
		delay(1000);

		AWWRITE4(sc, CCMU_SATA_CLK, CCMU_SCLK_GATING);
		break;
	case CCMU_EMAC:
		AWSET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_EMAC);
		break;
	case CCMU_DMA:
		AWSET4(sc, CCMU_AHB_GATING0, CCMU_AHB_GATING_DMA);
		break;
	case CCMU_UART0:
	case CCMU_UART1:
	case CCMU_UART2:
	case CCMU_UART3:
	case CCMU_UART4:
	case CCMU_UART5:
	case CCMU_UART6:
	case CCMU_UART7:
		AWSET4(sc, CCMU_APB_GATING1, CCMU_APB_GATING_UARTx(mod - CCMU_UART0));
		break;
	default:
		break;
	}
}
