/*	$OpenBSD: apldog.c,v 1.4 2022/04/06 18:59:26 naddy Exp $	*/
/*
 * Copyright (c) 2021 Mark Kettenis <kettenis@openbsd.org>
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
#include <dev/ofw/fdt.h>

extern void (*cpuresetfn)(void);

/*
 * This driver is based on preliminary device tree bindings and will
 * almost certainly need changes once the official bindings land in
 * mainline Linux.  Support for these preliminary bindings will be
 * dropped as soon as official bindings are available.
 */

#define WDT_CHIP_CTL	0x000c
#define WDT_SYS_TMR	0x0010
#define WDT_SYS_RST	0x0014
#define  WDT_SYS_RST_IMMEDIATE	(1 << 0)
#define WDT_SYS_CTL	0x001c
#define  WDT_SYS_CTL_ENABLE	(1 << 2)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct apldog_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct apldog_softc *apldog_sc;

int	apldog_match(struct device *, void *, void *);
void	apldog_attach(struct device *, struct device *, void *);

const struct cfattach	apldog_ca = {
	sizeof (struct apldog_softc), apldog_match, apldog_attach
};

struct cfdriver apldog_cd = {
	NULL, "apldog", DV_DULL
};

void	apldog_reset(void);

int
apldog_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,wdt");
}

void
apldog_attach(struct device *parent, struct device *self, void *aux)
{
	struct apldog_softc *sc = (struct apldog_softc *)self;
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

	/* Reset the watchdog timers. */
	HWRITE4(sc, WDT_CHIP_CTL, 0);
	HWRITE4(sc, WDT_SYS_CTL, 0);

	apldog_sc = sc;
	if (cpuresetfn == NULL)
		cpuresetfn = apldog_reset;
}

void
apldog_reset(void)
{
	struct apldog_softc *sc = apldog_sc;

	/* Trigger a system reset. */
	HWRITE4(sc, WDT_SYS_RST, WDT_SYS_RST_IMMEDIATE);
	HWRITE4(sc, WDT_SYS_CTL, WDT_SYS_CTL_ENABLE);
	HWRITE4(sc, WDT_SYS_TMR, 0);

	delay(1000000);
}
