/*	$OpenBSD: palm_udc.c,v 1.3 2009/09/09 11:34:02 marex Exp $	*/

/*
 * Copyright (c) 2009 Marek Vasut <marex@openbsd.org>
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
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/timeout.h>

#include <dev/sdmmc/sdmmcreg.h>
#include <machine/machine_reg.h>
#include <machine/palm_var.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbf.h>
#include <dev/usb/usbfvar.h>

#include <arch/arm/xscale/pxa2x0_gpio.h>
#include <arch/arm/xscale/pxa27x_udc.h>

int	palm_udc_match(struct device *, void *, void *);
void	palm_udc_attach(struct device *, struct device *, void *);
int	palm_udc_detach(struct device *, int);
int	palm_udc_is_host(void);

struct cfattach pxaudc_palm_ca = {
	sizeof(struct pxaudc_softc),
	palm_udc_match,
	palm_udc_attach,
	palm_udc_detach,
};

int
palm_udc_match(struct device *parent, void *match, void *aux)
{
	if (mach_is_palmld || mach_is_palmtc)
		return 0;
	return pxaudc_match();
}

int
palm_udc_is_host(void)
{
	return 1;
}

void
palm_udc_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)self;

	if (mach_is_palmtx)
		sc->sc_gpio_detect	= GPIO13_PALMTX_USB_DETECT;
	else if (mach_is_palmt5 || mach_is_palmz72)
		sc->sc_gpio_detect	= GPIO15_USB_DETECT;
	else {
		printf(": No suitable GPIO setup found\n");
		return;
	}

	sc->sc_gpio_detect_inv	= 1;
	sc->sc_gpio_pullup	= GPIO95_USB_PULLUP;
	sc->sc_gpio_pullup_inv	= 0;
	sc->sc_is_host		= palm_udc_is_host;

	pxa2x0_gpio_set_function(sc->sc_gpio_detect, GPIO_IN);
	pxa2x0_gpio_set_function(GPIO95_USB_PULLUP, GPIO_OUT | GPIO_SET);

	pxa2x0_gpio_set_function(45, GPIO_OUT);
	pxa2x0_gpio_set_function(40, GPIO_OUT);
	pxa2x0_gpio_set_function(39, GPIO_IN);
	pxa2x0_gpio_set_function(38, GPIO_IN);
	pxa2x0_gpio_set_function(37, GPIO_OUT);
	pxa2x0_gpio_set_function(36, GPIO_IN);
	pxa2x0_gpio_set_function(34, GPIO_IN);
	pxa2x0_gpio_set_function(89, GPIO_OUT);
	pxa2x0_gpio_set_function(120, GPIO_OUT);

	pxaudc_attach(sc, aux);
}

int
palm_udc_detach(struct device *self, int flags)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)self;

	return pxaudc_detach(sc, flags);
}
