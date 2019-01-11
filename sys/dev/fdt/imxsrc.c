/* $OpenBSD: imxsrc.c,v 1.1 2019/01/11 08:02:19 patrick Exp $ */
/*
 * Copyright (c) 2019 Patrick Wildt <patrick@blueri.se>
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/cpufunc.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/fdt.h>

#define IMX8M_RESET_PCIEPHY			20
#define IMX8M_RESET_PCIEPHY_PERST		21
#define IMX8M_RESET_PCIE_CTRL_APPS_EN		22
#define IMX8M_RESET_PCIE_CTRL_APPS_TURNOFF	25
#define IMX8M_RESET_PCIE_CTRL_APPS_CLK_REQ	26
#define IMX8M_RESET_PCIE2PHY			33
#define IMX8M_RESET_PCIE2PHY_PERST		34
#define IMX8M_RESET_PCIE2_CTRL_APPS_EN		35
#define IMX8M_RESET_PCIE2_CTRL_APPS_CLK_REQ	36
#define IMX8M_RESET_PCIE2_CTRL_APPS_TURNOFF	37

#define SRC_PCIE1_RCR				0x2c
#define SRC_PCIE2_RCR				0x48
#define  SRC_PCIE_RCR_PCIEPHY_G_RST			(1 << 1)
#define  SRC_PCIE_RCR_PCIEPHY_BTN			(1 << 2)
#define  SRC_PCIE_RCR_PCIEPHY_PERST			(1 << 3)
#define  SRC_PCIE_RCR_PCIE_CTRL_APPS_CLK_REQ		(1 << 4)
#define  SRC_PCIE_RCR_PCIE_CTRL_APPS_EN			(1 << 6)
#define  SRC_PCIE_RCR_PCIE_CTRL_APPS_TURNOFF		(1 << 11)

struct imxsrc_reset {
	uint32_t	reg;
	uint32_t	bit;
};

struct imxsrc_reset imx8m_resets[] = {
	[IMX8M_RESET_PCIEPHY] = { SRC_PCIE1_RCR, SRC_PCIE_RCR_PCIEPHY_G_RST | SRC_PCIE_RCR_PCIEPHY_BTN },
	[IMX8M_RESET_PCIEPHY_PERST] = { SRC_PCIE1_RCR, SRC_PCIE_RCR_PCIEPHY_PERST },
	[IMX8M_RESET_PCIE_CTRL_APPS_EN] = { SRC_PCIE1_RCR, SRC_PCIE_RCR_PCIE_CTRL_APPS_EN },
	[IMX8M_RESET_PCIE_CTRL_APPS_TURNOFF] = { SRC_PCIE1_RCR, SRC_PCIE_RCR_PCIE_CTRL_APPS_TURNOFF },
	[IMX8M_RESET_PCIE_CTRL_APPS_CLK_REQ] = { SRC_PCIE1_RCR, SRC_PCIE_RCR_PCIE_CTRL_APPS_CLK_REQ },
	[IMX8M_RESET_PCIE2PHY] = { SRC_PCIE2_RCR, SRC_PCIE_RCR_PCIEPHY_G_RST | SRC_PCIE_RCR_PCIEPHY_BTN },
	[IMX8M_RESET_PCIE2PHY_PERST] = { SRC_PCIE2_RCR, SRC_PCIE_RCR_PCIEPHY_PERST },
	[IMX8M_RESET_PCIE2_CTRL_APPS_EN] = { SRC_PCIE2_RCR, SRC_PCIE_RCR_PCIE_CTRL_APPS_EN },
	[IMX8M_RESET_PCIE2_CTRL_APPS_CLK_REQ] = { SRC_PCIE2_RCR, SRC_PCIE_RCR_PCIE_CTRL_APPS_CLK_REQ },
	[IMX8M_RESET_PCIE2_CTRL_APPS_TURNOFF] = { SRC_PCIE2_RCR, SRC_PCIE_RCR_PCIE_CTRL_APPS_TURNOFF },
};

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct imxsrc_softc {
	struct device		 sc_dev;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	struct reset_device	 sc_rd;
	struct imxsrc_reset	*sc_resets;
	int			 sc_nresets;
};

int imxsrc_match(struct device *, void *, void *);
void imxsrc_attach(struct device *, struct device *, void *);
void imxsrc_reset(void *, uint32_t *, int);

struct cfattach	imxsrc_ca = {
	sizeof (struct imxsrc_softc), imxsrc_match, imxsrc_attach
};

struct cfdriver imxsrc_cd = {
	NULL, "imxsrc", DV_DULL
};

int
imxsrc_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "fsl,imx8mq-src");
}

void
imxsrc_attach(struct device *parent, struct device *self, void *aux)
{
	struct imxsrc_softc *sc = (struct imxsrc_softc *)self;
	struct fdt_attach_args *faa = aux;

	KASSERT(faa->fa_nreg >= 1);

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	sc->sc_resets = imx8m_resets;
	sc->sc_nresets = nitems(imx8m_resets);

	sc->sc_rd.rd_node = faa->fa_node;
	sc->sc_rd.rd_cookie = sc;
	sc->sc_rd.rd_reset = imxsrc_reset;
	reset_register(&sc->sc_rd);

	printf("\n");
}

void
imxsrc_reset(void *cookie, uint32_t *cells, int assert)
{
	struct imxsrc_softc *sc = cookie;
	int idx = cells[0];
	uint32_t reg;

	if (idx >= sc->sc_nresets || sc->sc_resets[idx].bit == 0) {
		printf("%s: 0x%08x\n", __func__, idx);
		return;
	}

	switch (idx) {
	case IMX8M_RESET_PCIEPHY:
	case IMX8M_RESET_PCIE2PHY:
		if (!assert)
			delay(10);
		break;
	case IMX8M_RESET_PCIE_CTRL_APPS_EN:
	case IMX8M_RESET_PCIE2_CTRL_APPS_EN:
		assert = !assert;
		break;
	}

	reg = HREAD4(sc, sc->sc_resets[idx].reg);
	if (assert)
		reg |= sc->sc_resets[idx].bit;
	else
		reg &= ~sc->sc_resets[idx].bit;
	HWRITE4(sc, sc->sc_resets[idx].reg, reg);
}
