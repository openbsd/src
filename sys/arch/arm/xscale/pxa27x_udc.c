/*	$OpenBSD: pxa27x_udc.c,v 1.2 2005/02/18 16:42:09 dlg Exp $ */

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/kthread.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <arm/xscale/pxa27x_udcreg.h>

int	pxaudc_match(struct device *, void *, void *);
void	pxaudc_attach(struct device *, struct device *, void *);
int	pxaudc_detach(struct device *, int);

struct pxaudc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_size;
};

struct cfattach pxaudc_ca = {
	sizeof(struct pxaudc_softc), pxaudc_match, pxaudc_attach,
	pxaudc_detach
};

struct cfdriver pxaudc_cd = {
	NULL, "pxaudc", DV_DULL
};

int
pxaudc_match(struct device *parent, void *match, void *aux)
{
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) != CPU_ID_PXA27X)
		return (0);

	return (1);
}

void
pxaudc_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxaudc_softc		*sc = (struct pxaudc_softc *)self;
	struct pxaip_attach_args	*pxa = aux;
	u_int32_t			hr;

	sc->sc_iot = pxa->pxa_iot;
	sc->sc_size = 0;

	if (bus_space_map(sc->sc_iot, PXA2X0_USBDC_BASE, PXA2X0_USBDC_SIZE, 0,
	    &sc->sc_ioh)) {
		printf(": cannot map mem space\n");
		return;
	}
	sc->sc_size = PXA2X0_USBDC_SIZE;

	printf(": USB Device Controller\n");

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, 0, sc->sc_size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	pxa2x0_clkman_config(CKEN_USBDC, 0);

	/* disable the controller */
	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UDCCR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UDCCR,
	    hr & ~USBDC_UDCCR_UDE);

	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UDCICR1);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UDCICR1,
	    hr | USBDC_UDCICR1_IERS);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR, 0);
	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR,
	    hr | USBDC_UP2OCR_HXS);
	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR,
	    hr | USBDC_UP2OCR_HXOE);
	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR,
	    hr | USBDC_UP2OCR_DPPDE|USBDC_UP2OCR_DMPDE);

	pxa2x0_gpio_set_bit(37);
}

int
pxaudc_detach(struct device *self, int flags)
{
	struct pxaudc_softc		*sc = (struct pxaudc_softc *)self;

	if (sc->sc_size) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_size = 0;
	}

	return (0);
}
