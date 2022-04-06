/*	$OpenBSD: rkrng.c,v 1.4 2022/04/06 18:59:28 naddy Exp $	*/
/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/fdt.h>

/* Registers */
#define RNG_CTRL			0x0008
#define  RNG_CTRL_START			(1 << 8)
#define RNG_TRNG_CTRL			0x0200
#define  RNG_TRNG_CTRL_OSC_ENABLE	(1 << 16)
#define  RNG_TRNG_CTRL_SAMPLE_PERIOD(x)	(x)
#define RNG_DATA0			0x0204

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct rkrng_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct timeout		sc_to;
	int			sc_started;
};

int	rkrng_match(struct device *, void *, void *);
void	rkrng_attach(struct device *, struct device *, void *);

const struct cfattach rkrng_ca = {
	sizeof (struct rkrng_softc), rkrng_match, rkrng_attach
};

struct cfdriver rkrng_cd = {
	NULL, "rkrng", DV_DULL
};

void	rkrng_rnd(void *);

int
rkrng_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "rockchip,cryptov1-rng");
}

void
rkrng_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkrng_softc *sc = (struct rkrng_softc *)self;
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

	clock_set_assigned(faa->fa_node);
	clock_enable_all(faa->fa_node);

	timeout_set(&sc->sc_to, rkrng_rnd, sc);
	rkrng_rnd(sc);
}

void
rkrng_rnd(void *arg)
{
	struct rkrng_softc *sc = arg;
	bus_size_t off;

	if (!sc->sc_started) {
		HWRITE4(sc, RNG_TRNG_CTRL, RNG_TRNG_CTRL_OSC_ENABLE |
		    RNG_TRNG_CTRL_SAMPLE_PERIOD(100));
		HWRITE4(sc, RNG_CTRL, (RNG_CTRL_START << 16) | RNG_CTRL_START);
		sc->sc_started = 1;
		timeout_add_usec(&sc->sc_to, 100);
		return;
	}

	if (HREAD4(sc, RNG_CTRL) & RNG_CTRL_START) {
		timeout_add_usec(&sc->sc_to, 100);
		return;
	}

	for (off = 0; off < 32; off += 4)
		enqueue_randomness(HREAD4(sc, RNG_DATA0 + off));

	HWRITE4(sc, RNG_CTRL, (RNG_CTRL_START << 16) | 0);
	sc->sc_started = 0;

	timeout_add_sec(&sc->sc_to, 1);
}
