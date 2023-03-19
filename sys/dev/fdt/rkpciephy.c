/*	$OpenBSD: rkpciephy.c,v 1.1 2023/03/19 11:17:16 kettenis Exp $	*/
/*
 * Copyright (c) 2023 Mark Kettenis <kettenis@openbsd.org>
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

/* GRF registers */
#define GRF_PCIE30PHY_CON(idx)			((idx) * 4)
/* CON1 */
#define  GRF_PCIE30PHY_DA_OCM			0x80008000
/* CON5 */
#define  GRF_PCIE30PHY_LANE0_LINK_NUM_MASK	(0xf << 16)
#define  GRF_PCIE30PHY_LANE0_LINK_NUM_SHIFT	0
/* CON6 */
#define  GRF_PCIE30PHY_LANE1_LINK_NUM_MASK	(0xf << 16)
#define  GRF_PCIE30PHY_LANE1_LINK_NUM_SHIFT	0
/* STATUS0 */
#define GRF_PCIE30PHY_STATUS0			0x80
#define  GRF_PCIE30PHY_SRAM_INIT_DONE		(1 << 14)

struct rkpciephy_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct phy_device	sc_pd;
};

int	rkpciephy_match(struct device *, void *, void *);
void	rkpciephy_attach(struct device *, struct device *, void *);

const struct cfattach rkpciephy_ca = {
	sizeof (struct rkpciephy_softc), rkpciephy_match, rkpciephy_attach
};

struct cfdriver rkpciephy_cd = {
	NULL, "rkpciephy", DV_DULL
};

int	rkpciephy_enable(void *, uint32_t *);

int
rkpciephy_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "rockchip,rk3568-pcie3-phy");
}

void
rkpciephy_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkpciephy_softc *sc = (struct rkpciephy_softc *)self;
	struct fdt_attach_args *faa = aux;

	printf("\n");

	sc->sc_pd.pd_node = faa->fa_node;
	sc->sc_pd.pd_cookie = sc;
	sc->sc_pd.pd_enable = rkpciephy_enable;
	phy_register(&sc->sc_pd);
}

int
rkpciephy_enable(void *cookie, uint32_t *cells)
{
	struct rkpciephy_softc *sc = cookie;
	struct regmap *rm;
	int node = sc->sc_pd.pd_node;
	uint32_t data_lanes[2] = { 0, 0 };
	uint32_t grf, stat;
	int timo;

	grf = OF_getpropint(node, "rockchip,phy-grf", 0);
	rm = regmap_byphandle(grf);
	if (rm == NULL)
		return ENXIO;

	clock_enable_all(node);
	reset_assert(node, "phy");
	delay(1);

	regmap_write_4(rm, GRF_PCIE30PHY_CON(9), GRF_PCIE30PHY_DA_OCM);

	OF_getpropintarray(node, "data-lanes", data_lanes, sizeof(data_lanes));
	if (data_lanes[0] > 0) {
		regmap_write_4(rm, GRF_PCIE30PHY_CON(5),
		    GRF_PCIE30PHY_LANE0_LINK_NUM_MASK |
		    (data_lanes[0] - 1) << GRF_PCIE30PHY_LANE0_LINK_NUM_SHIFT);
	}
	if (data_lanes[1] > 0) {
		regmap_write_4(rm, GRF_PCIE30PHY_CON(6),
		    GRF_PCIE30PHY_LANE1_LINK_NUM_MASK |
		    (data_lanes[1] - 1) << GRF_PCIE30PHY_LANE1_LINK_NUM_SHIFT);
	}
	if (data_lanes[0] > 1 || data_lanes[1] > 1)
		regmap_write_4(rm, GRF_PCIE30PHY_CON(1), GRF_PCIE30PHY_DA_OCM);

	reset_deassert(node, "phy");

	for (timo = 500; timo > 0; timo--) {
		stat = regmap_read_4(rm, GRF_PCIE30PHY_STATUS0);
		if (stat & GRF_PCIE30PHY_SRAM_INIT_DONE)
			break;
		delay(100);
	}
	if (timo == 0) {
		printf("%s: timeout\n", sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	return 0;
}
