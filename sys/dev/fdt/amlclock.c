/*	$OpenBSD: amlclock.c,v 1.2 2019/08/27 18:29:45 kettenis Exp $	*/
/*
 * Copyright (c) 2019 Mark Kettenis <kettenis@openbsd.org>
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

#define G12A_USB		47
#define G12A_PCIE_PLL		201

#define HHI_PCIE_PLL_CNTL0	0x26
#define HHI_PCIE_PLL_CNTL1	0x27
#define HHI_PCIE_PLL_CNTL2	0x28
#define HHI_PCIE_PLL_CNTL3	0x29
#define HHI_PCIE_PLL_CNTL4	0x2a
#define HHI_PCIE_PLL_CNTL5	0x2b
#define HHI_GCLK_MPEG1		0x51

#define HREAD4(sc, reg)							\
	(regmap_read_4((sc)->sc_rm, (reg) << 2))
#define HWRITE4(sc, reg, val)						\
	regmap_write_4((sc)->sc_rm, (reg) << 2, (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct amlclock_softc {
	struct device		sc_dev;
	struct regmap		*sc_rm;

	struct clock_device	sc_cd;
};

int amlclock_match(struct device *, void *, void *);
void amlclock_attach(struct device *, struct device *, void *);

struct cfattach	amlclock_ca = {
	sizeof (struct amlclock_softc), amlclock_match, amlclock_attach
};

struct cfdriver amlclock_cd = {
	NULL, "amlclock", DV_DULL
};

uint32_t amlclock_get_frequency(void *, uint32_t *);
int	amlclock_set_frequency(void *, uint32_t *, uint32_t);
void	amlclock_enable(void *, uint32_t *, int);

int
amlclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "amlogic,g12a-clkc") ||
	    OF_is_compatible(faa->fa_node, "amlogic,g12b-clkc"));
}

void
amlclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct amlclock_softc *sc = (struct amlclock_softc *)self;
	struct fdt_attach_args *faa = aux;

	sc->sc_rm = regmap_bynode(OF_parent(faa->fa_node));
	if (sc->sc_rm == NULL) {
		printf(": no registers\n");
		return;
	}

	printf("\n");

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	sc->sc_cd.cd_get_frequency = amlclock_get_frequency;
	sc->sc_cd.cd_set_frequency = amlclock_set_frequency;
	sc->sc_cd.cd_enable = amlclock_enable;
	clock_register(&sc->sc_cd);
}

uint32_t
amlclock_get_frequency(void *cookie, uint32_t *cells)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
amlclock_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	struct amlclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case G12A_PCIE_PLL:
		/* Fixed at 100 MHz. */
		if (freq != 100000000)
			return -1;
		HWRITE4(sc, HHI_PCIE_PLL_CNTL0, 0x20090496);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL0, 0x30090496);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL1, 0x00000000);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL2, 0x00001100);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL3, 0x10058e00);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL4, 0x000100c0);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL5, 0x68000048);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL5, 0x68000068);
		delay(20);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL4, 0x008100c0);
		delay(10);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL0, 0x34090496);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL0, 0x14090496);
		delay(10);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL2, 0x00001000);
		return 0;
	};

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}

void
amlclock_enable(void *cookie, uint32_t *cells, int on)
{
	struct amlclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case G12A_USB:
		if (on)
			HSET4(sc, HHI_GCLK_MPEG1, (1 << 26));
		else
			HCLR4(sc, HHI_GCLK_MPEG1, (1 << 26));
		return;
	case G12A_PCIE_PLL:
		/* Already enabled. */
		return;
	}

	printf("%s: 0x%08x\n", __func__, idx);
}
