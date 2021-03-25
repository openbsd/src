/* $OpenBSD: imxdog.c,v 1.6 2021/03/25 04:12:01 jsg Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

#include <armv7/armv7/armv7_machdep.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

/* registers */
#define WCR		0x00
#define WSR		0x02
#define WRSR		0x04
#define WICR		0x06
#define WMCR		0x08

struct imxdog_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct imxdog_softc *imxdog_sc;

int	imxdog_match(struct device *, void *, void *);
void	imxdog_attach(struct device *, struct device *, void *);
void	imxdog_reset(void);

struct cfattach	imxdog_ca = {
	sizeof (struct imxdog_softc), imxdog_match, imxdog_attach
};

struct cfdriver imxdog_cd = {
	NULL, "imxdog", DV_DULL
};

int
imxdog_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "fsl,imx21-wdt");
}

void
imxdog_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct imxdog_softc *sc = (struct imxdog_softc *) self;

	if (faa->fa_nreg < 1)
		return;

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("imxdog_attach: bus_space_map failed!");

	printf("\n");

	imxdog_sc = sc;
	cpuresetfn = imxdog_reset;
}

void
imxdog_reset(void)
{
	if (imxdog_sc == NULL)
		return;

	/* disable watchdog and set timeout to 0 */
	bus_space_write_2(imxdog_sc->sc_iot, imxdog_sc->sc_ioh, WCR, 0);

	/* sequence to reset timeout counter */
	bus_space_write_2(imxdog_sc->sc_iot, imxdog_sc->sc_ioh, WSR, 0x5555);
	bus_space_write_2(imxdog_sc->sc_iot, imxdog_sc->sc_ioh, WSR, 0xaaaa);

	/* enable watchdog */
	bus_space_write_2(imxdog_sc->sc_iot, imxdog_sc->sc_ioh, WCR, 1);
	/* errata TKT039676 */
	bus_space_write_2(imxdog_sc->sc_iot, imxdog_sc->sc_ioh, WCR, 1);

	delay(100000);
}
