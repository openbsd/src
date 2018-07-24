/*	$OpenBSD: mvclock.c,v 1.2 2018/07/24 21:52:38 kettenis Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis <kettenis@openbsd.org>
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

struct mvclock_softc {
	struct device		sc_dev;

	struct clock_device	sc_cd;
};

int mvclock_match(struct device *, void *, void *);
void mvclock_attach(struct device *, struct device *, void *);

struct cfattach	mvclock_ca = {
	sizeof (struct mvclock_softc), mvclock_match, mvclock_attach
};

struct cfdriver mvclock_cd = {
	NULL, "mvclock", DV_DULL
};

uint32_t ap806_get_frequency(void *, uint32_t *);
uint32_t cp110_get_frequency(void *, uint32_t *);
void	cp110_enable(void *, uint32_t *, int);

int
mvclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "marvell,ap806-clock") ||
	    OF_is_compatible(faa->fa_node, "marvell,cp110-clock"));
}

void
mvclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct mvclock_softc *sc = (struct mvclock_softc *)self;
	struct fdt_attach_args *faa = aux;

	printf("\n");

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	if (OF_is_compatible(faa->fa_node, "marvell,ap806-clock")) {
		sc->sc_cd.cd_get_frequency = ap806_get_frequency;
	} else {
		sc->sc_cd.cd_get_frequency = cp110_get_frequency;
		sc->sc_cd.cd_enable = cp110_enable;
	}
	clock_register(&sc->sc_cd);
}

/* AP806 block */

#define AP806_CORE_FIXED	2
#define AP806_CORE_MSS		3
#define AP806_CORE_SDIO		4

uint32_t
ap806_get_frequency(void *cookie, uint32_t *cells)
{
	uint32_t idx = cells[0];

	switch (idx) {
	case AP806_CORE_FIXED:
		/* fixed PLL at 1200MHz */
		return 1200000000;
	case AP806_CORE_MSS:
		/* MSS clock is fixed clock divided by 6 */
		return 200000000;
	case AP806_CORE_SDIO:
		/* SDIO/eMMC clock is fixed clock divided by 3 */
		return 400000000;
	default:
		break;
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

/* CP110 block */

#define CP110_PM_CLOCK_GATING_CTRL	0x220

#define CP110_CORE_APLL		0
#define CP110_CORE_PPV2		1
#define CP110_CORE_X2CORE	2
#define CP110_CORE_CORE		3
#define CP110_CORE_SDIO		5

#define CP110_GATE_SDIO		4
#define CP110_GATE_SLOW_IO	21

uint32_t
cp110_get_frequency(void *cookie, uint32_t *cells)
{
	struct mvclock_softc *sc = cookie;
	uint32_t mod = cells[0];
	uint32_t idx = cells[1];
	uint32_t parent[2] = { 0, 0 };

	/* Core clocks */
	if (mod == 0) {
		switch (idx) {
		case CP110_CORE_APLL:
			/* fixed PLL at 1GHz */
			return 1000000000;
		case CP110_CORE_PPV2:
			/* PPv2 clock is APLL/3 */
			return 333333333;
		case CP110_CORE_X2CORE:
			/* X2CORE clock is APLL/2 */
			return 500000000;
		case CP110_CORE_CORE:
			/* Core clock is X2CORE/2 */
			return 250000000;
		case CP110_CORE_SDIO:
			/* SDIO clock is APLL/2.5 */
			return 400000000;
		default:
			break;
		}
	}

	/* Gatable clocks */
	if (mod == 1) {
		switch (idx) {
		case CP110_GATE_SDIO:
			parent[1] = CP110_CORE_SDIO;
			break;
		case CP110_GATE_SLOW_IO:
			parent[1] = CP110_CORE_X2CORE;
			break;
		default:
			break;
		}

		if (parent[1] != 0)
			return cp110_get_frequency(sc, parent);
	}

	printf("%s: 0x%08x 0x%08x\n", __func__, mod, idx);
	return 0;
}

void
cp110_enable(void *cookie, uint32_t *cells, int on)
{
	struct mvclock_softc *sc = cookie;
	uint32_t mod = cells[0];
	uint32_t idx = cells[1];

	/* Gatable clocks */
	if (mod == 1 && idx < 32) {
		struct regmap *rm;
		uint32_t reg;

		rm = regmap_bynode(OF_parent(sc->sc_cd.cd_node));
		if (rm == NULL) {
			printf("%s: can't enable clock 0x%08x 0x%08x\n",
			    sc->sc_dev.dv_xname, mod, idx);
			return;
		}
		reg = regmap_read_4(rm, CP110_PM_CLOCK_GATING_CTRL);
		if (on)
			reg |= (1U << idx);
		else
			reg &= ~(1U << idx);
		regmap_write_4(rm, CP110_PM_CLOCK_GATING_CTRL, reg);
		return;
	}

	printf("%s: 0x%08x 0x%08x\n", __func__, mod, idx);
}
