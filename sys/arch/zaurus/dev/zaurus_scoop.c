/*	$OpenBSD: zaurus_scoop.c,v 1.1 2005/01/19 02:02:34 uwe Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@bsdx.de>
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
#include <sys/conf.h>

#include <machine/bus.h>

#include <arm/xscale/pxa2x0var.h>

#include <zaurus/dev/zaurus_scoopreg.h>
#include <zaurus/dev/zaurus_scoopvar.h>

int	scoopmatch(struct device *, void *, void *);
void	scoopattach(struct device *, struct device *, void *);

struct cfattach scoop_pxaip_ca = {
	sizeof (struct scoop_softc), scoopmatch, scoopattach
};

struct cfdriver scoop_cd = {
	NULL, "scoop", DV_DULL
};

/* GPIO pin/bit numbers for the Zaurus C3000. */
#define SCOOP1_BACKLIGHT_ON     8

struct scoop_softc *scoop0_sc;
struct scoop_softc *scoop1_sc;
int nscoopunits = 0;


int
scoopmatch(struct device *parent, void *cf, void *aux)
{
	struct pxaip_attach_args *pxa = aux;

	if (pxa->pxa_addr == -1)
		return 0;

	/*
	 * Only the C3000 models (pxa270) are known to have two SCOOPs,
	 * on other models we only find the first one.
	 */
        if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X)
	    	return (nscoopunits < 2);

	return (nscoopunits < 1);
}

void
scoopattach(struct device *parent, struct device *self, void *aux)
{
	struct pxaip_attach_args *pxa = aux;
	struct scoop_softc *sc = (struct scoop_softc *)self;
	bus_size_t size;

	nscoopunits++;

	sc->sc_iot = pxa->pxa_iot;
	size = pxa->pxa_size < SCOOP_SIZE ? SCOOP_SIZE : pxa->pxa_size;

	if (bus_space_map(sc->sc_iot, pxa->pxa_addr, size, 0,
	    &sc->sc_ioh) != 0) {
		printf(": failed to map SCOOP%d\n", nscoopunits - 1);
		return;
	}

	printf(": Onboard Peripheral Controller\n");

	if (nscoopunits == 1)
		scoop0_sc = sc;
	else if (nscoopunits == 2)
		scoop1_sc = sc;
}

int
scoop_gpio_pin_read(struct scoop_softc *sc, int pin)
{
	unsigned short rv;
	unsigned short bit = (1 << pin);

	rv = bus_space_read_2(sc->sc_iot, sc->sc_ioh, SCOOP_GPWR);
	return (rv & bit) != 0 ? 1 : 0;
}

void
scoop_gpio_pin_write(struct scoop_softc *sc, int pin, int value)
{
	unsigned short rv;
	unsigned short bit = (1 << pin);

	rv = bus_space_read_2(sc->sc_iot, sc->sc_ioh, SCOOP_GPWR);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, SCOOP_GPWR,
	    value != 0 ? (rv | bit) : (rv & ~bit));
}

void
scoop_backlight_on(int enable)
{

#if 0	/* XXX no effect. maybe the pin is incorrectly configured? */
	if (scoop1_sc != (struct scoop_softc *)0)
		scoop_gpio_pin_write(scoop1_sc, SCOOP1_BACKLIGHT_ON,
		    enable);
#endif
}
