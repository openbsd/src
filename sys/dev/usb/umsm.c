/*	$OpenBSD: umsm.c,v 1.13 2007/06/12 16:26:37 mbalmer Exp $	*/

/*
 * Copyright (c) 2006 Jonathan Gray <jsg@openbsd.org>
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

/* Driver for Qualcomm MSM EVDO and UMTS communication devices */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/ucomvar.h>

#define UMSMBUFSZ	2048
#define UMSM_CONFIG_NO	0
#define UMSM_IFACE_NO	0

struct umsm_softc {
	struct device		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;
	struct device *		sc_subdev;
	u_char			sc_dying;
};

struct ucom_methods umsm_methods = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static const struct usb_devno umsm_devs[] = {
	{ USB_VENDOR_AIRPRIME,	USB_PRODUCT_AIRPRIME_PC5220 },
	{ USB_VENDOR_DELL,	USB_PRODUCT_DELL_W5500 },
	{ USB_VENDOR_KYOCERA2,	USB_PRODUCT_KYOCERA2_KPC650 },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_EXPRESSCARD },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_MERLINV620 },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_S720 },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_U720 },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_XU870 },
	{ USB_VENDOR_QUALCOMM,	USB_PRODUCT_QUALCOMM_MSM_HSDPA },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_EM5625 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_AIRCARD_580 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_AIRCARD_595 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_AIRCARD_875 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC5720 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC5725 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC8755 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC8755_2 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC8765 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC8775 },
};

USB_DECLARE_DRIVER(umsm);

int
umsm_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (usb_lookup(umsm_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

void
umsm_attach(struct device *parent, struct device *self, void *aux)
{
	struct umsm_softc *sc = (struct umsm_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct ucom_attach_args uca;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	char *devinfop;
	int i;

	bzero(&uca, sizeof(uca));
	sc->sc_udev = uaa->device;
	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	printf("\n%s: %s\n", sc->sc_dev.dv_xname, devinfop);
	usbd_devinfo_free(devinfop);

	if (usbd_set_config_index(sc->sc_udev, UMSM_CONFIG_NO, 1) != 0) {
		printf("%s: could not set configuration no\n",
		    sc->sc_dev.dv_xname);
		sc->sc_dying = 1;
		return;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, UMSM_IFACE_NO,
	    &sc->sc_iface);
	if (error != 0) {
		printf("%s: could not get interface handle\n",
		    sc->sc_dev.dv_xname);
		sc->sc_dying = 1;
		return;
	}

	id = usbd_get_interface_descriptor(sc->sc_iface);

	uca.bulkin = uca.bulkout = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor found for %d\n",
			    sc->sc_dev.dv_xname, i);
			sc->sc_dying = 1;
			return;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			uca.bulkin = ed->bEndpointAddress;
		else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			uca.bulkout = ed->bEndpointAddress;
	}
	if (uca.bulkin == -1 || uca.bulkout == -1) {
		printf("%s: missing endpoint\n", sc->sc_dev.dv_xname);
		sc->sc_dying = 1;
		return;
	}

	/* We need to force size as some devices lie */
	uca.ibufsize = UMSMBUFSZ;
	uca.obufsize = UMSMBUFSZ;
	uca.ibufsizepad = UMSMBUFSZ;
	uca.opkthdrlen = 0;
	uca.device = sc->sc_udev;
	uca.iface = sc->sc_iface;
	uca.methods = &umsm_methods;
	uca.arg = sc;
	uca.info = NULL;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    &sc->sc_dev);
	
	sc->sc_subdev = config_found_sm(self, &uca, ucomprint, ucomsubmatch);
}

int
umsm_detach(struct device *self, int flags)
{
	struct umsm_softc *sc = (struct umsm_softc *)self;
	int rv = 0;

	sc->sc_dying = 1;
	if (sc->sc_subdev != NULL) {
		rv = config_detach(sc->sc_subdev, flags);
		sc->sc_subdev = NULL;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   &sc->sc_dev);

	return (rv);
}

int
umsm_activate(struct device *self, enum devact act)
{
	struct umsm_softc *sc = (struct umsm_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_subdev != NULL)
			rv = config_deactivate(sc->sc_subdev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}
