/*	$OpenBSD: syscon.c,v 1.4 2018/03/17 18:04:15 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis
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

extern void (*cpuresetfn)(void);
extern void (*powerdownfn)(void);

struct syscon_softc {
	struct simplebus_softc	sc_sbus;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	uint32_t		sc_regmap;
	bus_size_t		sc_offset;
	uint32_t		sc_mask;
};

struct syscon_softc *syscon_reboot_sc;
struct syscon_softc *syscon_poweroff_sc;

int	syscon_match(struct device *, void *, void *);
void	syscon_attach(struct device *, struct device *, void *);

struct cfattach syscon_ca = {
	sizeof(struct syscon_softc), syscon_match, syscon_attach
};

struct cfdriver syscon_cd = {
	NULL, "syscon", DV_DULL
};

void	syscon_reset(void);
void	syscon_powerdown(void);

int
syscon_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "syscon") ||
	    OF_is_compatible(faa->fa_node, "syscon-reboot") ||
	    OF_is_compatible(faa->fa_node, "syscon-poweroff");
}

void
syscon_attach(struct device *parent, struct device *self, void *aux)
{
	struct syscon_softc *sc = (struct syscon_softc *)self;
	struct fdt_attach_args *faa = aux;
	char name[32];

	OF_getprop(faa->fa_node, "name", name, sizeof(name));
	name[sizeof(name) - 1] = 0;

	if (OF_is_compatible(faa->fa_node, "syscon")) {
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

		regmap_register(faa->fa_node, sc->sc_iot, sc->sc_ioh,
		    faa->fa_reg[0].size);
	}

	if (OF_is_compatible(faa->fa_node, "simple-mfd"))
		simplebus_attach(parent, &sc->sc_sbus.sc_dev, faa);
	else
		printf(": \"%s\"\n", name);

	if (OF_is_compatible(faa->fa_node, "syscon-reboot") ||
	    OF_is_compatible(faa->fa_node, "syscon-poweroff")) {
		sc->sc_regmap = OF_getpropint(faa->fa_node, "regmap", 0);
		if (sc->sc_regmap == 0)
			return;

		if (OF_getproplen(faa->fa_node, "offset") != sizeof(uint32_t) ||
		    OF_getproplen(faa->fa_node, "mask") != sizeof(uint32_t))
			return;

		sc->sc_offset = OF_getpropint(faa->fa_node, "offset", 0);
		sc->sc_mask = OF_getpropint(faa->fa_node, "mask", 0);

		if (OF_is_compatible(faa->fa_node, "syscon-reboot")) {
			syscon_reboot_sc = sc;
			cpuresetfn = syscon_reset;
		} else if (OF_is_compatible(faa->fa_node, "syscon-poweroff")) {
			syscon_poweroff_sc = sc;
			powerdownfn = syscon_powerdown;
		}
	}
}

void
syscon_reset(void)
{
	struct syscon_softc *sc = syscon_reboot_sc;
	struct regmap *rm;

	rm = regmap_byphandle(sc->sc_regmap);
	if (rm == NULL)
		return;

	regmap_write_4(rm, sc->sc_offset, sc->sc_mask);
	delay(1000000);
}

void
syscon_powerdown(void)
{
	struct syscon_softc *sc = syscon_poweroff_sc;
	struct regmap *rm;

	rm = regmap_byphandle(sc->sc_regmap);
	if (rm == NULL)
		return;

	regmap_write_4(rm, sc->sc_offset, sc->sc_mask);
	delay(1000000);
}
