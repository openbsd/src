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

/* Attachment driver for pxaudc(4) on Zaurus */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/timeout.h>

#include <dev/sdmmc/sdmmcreg.h>
#include <machine/machine_reg.h>
#include <machine/zaurus_var.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbf.h>
#include <dev/usb/usbfvar.h>

#include <arch/arm/xscale/pxa2x0_gpio.h>
#include <arch/arm/xscale/pxa27x_udc.h>

int	zaurus_udc_match(struct device *, void *, void *);
void	zaurus_udc_attach(struct device *, struct device *, void *);
int	zaurus_udc_detach(struct device *, int);
int	zaurus_udc_is_host(void);

struct cfattach pxaudc_zaurus_ca = {
	sizeof(struct pxaudc_softc),
	zaurus_udc_match,
	zaurus_udc_attach,
	zaurus_udc_detach,
};

int
zaurus_udc_match(struct device *parent, void *match, void *aux)
{
	return pxaudc_match();
}

int
zaurus_udc_is_host(void)
{
	return !(pxa2x0_gpio_get_bit(GPIO_USB_DETECT) ||
		pxa2x0_gpio_get_bit(GPIO_USB_DEVICE));
}

void
zaurus_udc_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)self;


	sc->sc_gpio_detect	= GPIO_USB_DETECT;
	sc->sc_gpio_pullup	= GPIO_USB_PULLUP;
	sc->sc_gpio_pullup_inv	= 0;
	sc->sc_is_host		= zaurus_udc_is_host;

	/* Platform specific GPIO configuration */
	pxa2x0_gpio_set_function(GPIO_USB_DETECT, GPIO_IN);
	pxa2x0_gpio_set_function(GPIO_USB_DEVICE, GPIO_IN);
	pxa2x0_gpio_set_function(GPIO_USB_PULLUP, GPIO_OUT);

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
zaurus_udc_detach(struct device *self, int flags)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)self;

	return pxaudc_detach(sc, flags);
}
