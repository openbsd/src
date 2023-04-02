/*	$OpenBSD: rkusbphy.c,v 1.1 2023/04/02 01:21:39 dlg Exp $ */

/*
 * Copyright (c) 2023 David Gwynne <dlg@openbsd.org>
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
 * Rockchip USB2PHY with Innosilicon IP
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/*
 * chip stuff
 */

struct rkusbphy_reg {
	bus_size_t			r_offs;
};

struct rkusbphy_port_regs {
	struct rkusbphy_reg		pr_phy_sus;
	struct rkusbphy_reg		pr_bvalid_det_en;
};

struct rkusbphy_chip {
	bus_addr_t			c_base_addr;
	struct rkusbphy_port_regs	c_regs;
};

static const struct rkusbphy_chip rkusbphy_rk3568[] = {
	{
		.c_base_addr = 0xfe8a0000,
	},
	{
		.c_base_addr = 0xfe8b0000,
	},
};

/*
 * driver stuff
 */

struct rkusbphy_softc {
	struct device			 sc_dev;
	const struct rkusbphy_chip	*sc_chip;
	struct regmap			*sc_grf;
	int				 sc_node;

	struct phy_device		 sc_otg_phy;
	struct phy_device		 sc_host_phy;
};
#define DEVNAME(_sc) ((_sc)->sc_dev.dv_xname)

static int	rkusbphy_match(struct device *, void *, void *);
static void	rkusbphy_attach(struct device *, struct device *, void *);

static int	rkusbphy_otg_phy_enable(void *, uint32_t *);
static int	rkusbphy_host_phy_enable(void *, uint32_t *);

struct rkusbphy_port_config {
	const char			*pc_name;
	int (*pc_enable)(void *, uint32_t *);
};

static void	rkusbphy_register(struct rkusbphy_softc *,
		    struct phy_device *, const struct rkusbphy_port_config *);

static const struct rkusbphy_port_config rkusbphy_otg_config = {
	.pc_name = "otg-port",
	.pc_enable = rkusbphy_otg_phy_enable,
};

static const struct rkusbphy_port_config rkusbphy_host_config = {
	.pc_name = "host-port",
	.pc_enable = rkusbphy_host_phy_enable,
};

const struct cfattach rkusbphy_ca = {
	sizeof (struct rkusbphy_softc), rkusbphy_match, rkusbphy_attach
};

struct cfdriver rkusbphy_cd = {
	NULL, "rkusbphy", DV_DULL
};

struct rkusbphy_id {
	const char			*id_name;
	const struct rkusbphy_chip	*id_chips;
	size_t				 id_nchips;
};

#define RKUSBPHY_ID(_n, _c) { _n, _c, nitems(_c) }

static const struct rkusbphy_id rkusbphy_ids[] = {
	RKUSBPHY_ID("rockchip,rk3568-usb2phy", rkusbphy_rk3568),
};

static const struct rkusbphy_id *
rkusbphy_lookup(struct fdt_attach_args *faa)
{
	size_t i;

	for (i = 0; i < nitems(rkusbphy_ids); i++) {
		const struct rkusbphy_id *id = &rkusbphy_ids[i];
		if (OF_is_compatible(faa->fa_node, id->id_name))
			return (id);
	}

	return (NULL);
}

static int
rkusbphy_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (rkusbphy_lookup(faa) != NULL ? 1 : 0);
}

static void
rkusbphy_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkusbphy_softc *sc = (struct rkusbphy_softc *)self;
	struct fdt_attach_args *faa = aux;
	const struct rkusbphy_id *id = rkusbphy_lookup(faa);
	size_t i;
	uint32_t grfph;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	for (i = 0; i < id->id_nchips; i++) {
		const struct rkusbphy_chip *c = &id->id_chips[i];
		if (faa->fa_reg[0].addr == c->c_base_addr) {
			printf(": phy %zu\n", i);
			sc->sc_chip = c;
			break;
		}
	}
	if (sc->sc_chip == NULL) {
		printf(": unknown base address 0x%llu\n", faa->fa_reg[0].addr);
		return;
	}

	sc->sc_node = faa->fa_node;

	grfph = OF_getpropint(sc->sc_node, "rockchip,usbgrf", 0);
	sc->sc_grf = regmap_byphandle(grfph);
	if (sc->sc_grf == NULL) {
		printf("%s: rockchip,usbgrf 0x%x not found\n", DEVNAME(sc),
		    grfph);
		return;
	}

	reset_assert_all(sc->sc_node);

	rkusbphy_register(sc, &sc->sc_otg_phy, &rkusbphy_otg_config);
	rkusbphy_register(sc, &sc->sc_host_phy, &rkusbphy_host_config);
}

static void
rkusbphy_register(struct rkusbphy_softc *sc, struct phy_device *pd,
    const struct rkusbphy_port_config *pc)
{
	char status[32];
	int node;

	node = OF_getnodebyname(sc->sc_node, pc->pc_name);
	if (node == 0) {
		printf("%s: cannot find %s\n", DEVNAME(sc), pc->pc_name);
		return;
	}

	if (OF_getprop(node, "status", status, sizeof(status)) > 0 &&
	    strcmp(status, "disabled") == 0)
		return;

	pd->pd_node = node;
	pd->pd_cookie = sc;
	pd->pd_enable = pc->pc_enable;
	phy_register(pd);
}

static void
rkusbphy_phy_supply(struct rkusbphy_softc *sc, int node)
{
	int phandle;

	phandle = OF_getpropint(node, "phy-supply", 0);
	if (phandle == 0)
		return;

	regulator_enable(phandle);
}

static int
rkusbphy_otg_phy_enable(void *cookie, uint32_t *cells)
{
	struct rkusbphy_softc *sc = cookie;

	rkusbphy_phy_supply(sc, sc->sc_otg_phy.pd_node);

	return (EINVAL);
}

static int
rkusbphy_host_phy_enable(void *cookie, uint32_t *cells)
{
	struct rkusbphy_softc *sc = cookie;

	rkusbphy_phy_supply(sc, sc->sc_host_phy.pd_node);

	return (EINVAL);
}
