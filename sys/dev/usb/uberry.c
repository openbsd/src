/*	$OpenBSD: uberry.c,v 1.6 2007/06/06 19:25:49 mk Exp $	*/

/*-
 * Copyright (c) 2006 Theo de Raadt <deraadt@openbsd.org>
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
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/intr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

struct uberry_softc {
	struct device			sc_dev;
	usbd_device_handle		sc_udev;
	usbd_interface_handle		sc_iface;
};

#define UBERRY_CONFIG_NO		0

struct usb_devno const uberry_devices[] = {
	{ USB_VENDOR_RIM, USB_PRODUCT_RIM_BLACKBERRY }
};

USB_DECLARE_DRIVER(uberry);

int
uberry_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (usb_lookup(uberry_devices, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

void
uberry_attach(struct device *parent, struct device *self, void *aux)
{
	struct uberry_softc *sc = (struct uberry_softc *)self;
	struct usb_attach_arg *uaa = aux;
	char *devinfop;

	sc->sc_udev = uaa->device;

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	printf("\n%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	/* Enable the device, then it cannot idle, and will charge */
	if (usbd_set_config_no(sc->sc_udev, UBERRY_CONFIG_NO, 1) != 0) {
		printf("%s: could not set configuration no\n",
		    USBDEVNAME(sc->sc_dev));
		return;
	}
	printf("%s: Charging enabled\n", USBDEVNAME(sc->sc_dev));

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));
}

int
uberry_detach(struct device *self, int flags)
{
	struct uberry_softc *sc = (struct uberry_softc *)self;

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	return 0;
}

int
uberry_activate(device_ptr_t self, enum devact act)
{
	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		break;
	}
	return 0;
}
