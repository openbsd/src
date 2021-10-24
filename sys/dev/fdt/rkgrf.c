/*	$OpenBSD: rkgrf.c,v 1.5 2021/10/24 17:52:26 mpi Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#ifdef __armv7__
#include <arm/simplebus/simplebusvar.h>
#else
#include <arm64/dev/simplebusvar.h>
#endif

struct rkgrf_softc {
	struct simplebus_softc	sc_sbus;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

int rkgrf_match(struct device *, void *, void *);
void rkgrf_attach(struct device *, struct device *, void *);

const struct cfattach	rkgrf_ca = {
	sizeof (struct rkgrf_softc), rkgrf_match, rkgrf_attach
};

struct cfdriver rkgrf_cd = {
	NULL, "rkgrf", DV_DULL
};

int
rkgrf_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "rockchip,rk3288-grf") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3288-pmu") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3288-sgrf") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3308-grf") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3399-grf") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3399-pmugrf"));
}

void
rkgrf_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkgrf_softc *sc = (struct rkgrf_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;

	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	printf("\n");

	regmap_register(faa->fa_node, sc->sc_iot, sc->sc_ioh,
	    faa->fa_reg[0].size);

	/* Attach PHYs. */
	simplebus_attach(parent, &sc->sc_sbus.sc_dev, faa);
}
