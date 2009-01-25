/*	$OpenBSD: udfu.c,v 1.1 2009/01/25 02:00:25 fgsch Exp $	*/

/*
 * Copyright (c) 2009 Federico G. Schwindt <fgsch@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * DFU spec: http://www.usb.org/developers/devclass_docs/DFU_1.1.pdf
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/timeout.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#ifdef UDFU_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

#define RUNTIME_MODE		1

#define DFU_DETACH		UT_WRITE_CLASS_INTERFACE, 0
#define DFU_GETSTATE		UT_READ_CLASS_INTERFACE,  5
#define  DFU_STATE_appIDLE		0

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bmAttributes;
#define BWILLDETACH			8
	uWord		wDetachTimeOut;
	uWord		wTransferSize;
	uWord		bcdDFUVersion;
} __packed dfu_functional_descriptor_t;

#define UDFU_DETACH_TIMEOUT	1000	/* in milliseconds */

struct udfu_softc {
	struct device		sc_dev;
	usbd_device_handle	sc_udev;

	int			sc_iface_index;

	int			sc_will_detach;
	int			sc_detach_timeout;
};

int	udfu_match(struct device *, void *, void *);
void	udfu_attach(struct device *, struct device *, void *);
int	udfu_detach(struct device *, int);

void	udfu_parse_desc(struct udfu_softc *);
int	udfu_request(struct udfu_softc *, int, int, int, void *, size_t);

struct cfdriver udfu_cd = {
	NULL, "udfu", DV_DULL
};

const struct cfattach udfu_ca = {
	sizeof(struct udfu_softc),
	udfu_match,
	udfu_attach,
	udfu_detach
};

int
udfu_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL)
		return (UMATCH_NONE);

	if (id->bInterfaceClass == UICLASS_APPL_SPEC &&
	    id->bInterfaceSubClass == UISUBCLASS_FIRMWARE_DOWNLOAD &&
	    id->bInterfaceProtocol == RUNTIME_MODE)
		return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);

	return (UMATCH_NONE);
}

void
udfu_attach(struct device *parent, struct device *self, void *aux)
{
	struct udfu_softc *sc = (struct udfu_softc *)self;
	struct usb_attach_arg *uaa = aux;
	usbd_status err;
	u_int8_t state;

	sc->sc_udev = uaa->device;
	sc->sc_iface_index = uaa->iface->index;
	sc->sc_detach_timeout = UDFU_DETACH_TIMEOUT;

	/* Parse the DFU functional descriptor. */
	udfu_parse_desc(sc);

	/*
	 * GETSTATE is optional in Runtime mode. If it fails, assume
	 * appIDLE and hope for the best.
	 */
	if ((err = udfu_request(sc, DFU_GETSTATE, 0, &state, 1))) {
		printf("%s: could not get current state, "
		    "assuming appIDLE\n", sc->sc_dev.dv_xname);
		state = DFU_STATE_appIDLE;
	}

	switch (state) {
	case DFU_STATE_appIDLE:
		err = udfu_request(sc, DFU_DETACH,
		    min(UDFU_DETACH_TIMEOUT, sc->sc_detach_timeout),
		    NULL, 0);
		if (err)
			printf("%s: DFU_DETACH failed\n",
			    sc->sc_dev.dv_xname);
		break;

	default:
		printf("%s: unexpected state %d\n",
		    sc->sc_dev.dv_xname, state);
		err = 1;
		break;
	}

	if (!sc->sc_will_detach && err == 0)
		usb_needs_reattach(sc->sc_udev);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    &sc->sc_dev);
}

int
udfu_detach(struct device *self, int flags)
{
	struct udfu_softc *sc = (struct udfu_softc *)self;

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    &sc->sc_dev);
	return (0);
}

void
udfu_parse_desc(struct udfu_softc *sc)
{
	dfu_functional_descriptor_t *dd;
	const usb_descriptor_t *desc;
	usbd_desc_iter_t iter;

	usb_desc_iter_init(sc->sc_udev, &iter);
	while ((desc = usb_desc_iter_next(&iter))) {
		if (desc->bDescriptorType == UDESC_CS_DEVICE)
			break;
	}

	if (!desc)
		return;

	dd = (dfu_functional_descriptor_t *)desc;

	DPRINTF(("%s: %s: bLength=%d bDescriptorType=%d bmAttributes=%d "
	    "wDetachTimeOut=%d wTransferSize=%d bcdDFUVersion=%d\n",
	    sc->sc_dev.dv_xname, __func__, dd->bLength,
	    dd->bDescriptorType, dd->bmAttributes,
	    UGETW(dd->wDetachTimeOut), UGETW(dd->wTransferSize),
	    UGETW(dd->bcdDFUVersion)));

	sc->sc_will_detach = dd->bmAttributes & BWILLDETACH;
	sc->sc_detach_timeout = UGETW(dd->wDetachTimeOut);
}

int
udfu_request(struct udfu_softc *sc, int type, int cmd, int value,
    void *data, size_t datalen)
{
	usb_device_request_t req;

	req.bmRequestType = type;
	req.bRequest = cmd;
	USETW(req.wValue, value);
	USETW(req.wIndex, sc->sc_iface_index);
	USETW(req.wLength, datalen);

	return (usbd_do_request(sc->sc_udev, &req, data));
}
