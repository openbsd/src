/*	$OpenBSD: uts.c,v 1.1 2007/03/16 21:40:32 robert Exp $ */

/*
 * Copyright (c) 2007 Robert Nagy <robert@openbsd.org> 
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

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#define UTS_CONFIG_INDEX 0

struct uts_softc {
	USBBASEDEVICE		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;
	int			sc_iface_number;
	int			sc_product;

	int			sc_intr_number;
	usbd_pipe_handle	sc_intr_pipe;
	u_char			*sc_ibuf;
	int			sc_isize;

	device_ptr_t		sc_wsmousedev;

	int	sc_enabled;
	int	sc_buttons;
	int	sc_dying;
	int	sc_oldx;
	int	sc_oldy;
};

/* Settable via sysctl */
int	uts_rawmode;
struct utsscale {
	int	ts_minx;
	int	ts_maxx;
	int	ts_miny;
	int	ts_maxy;
	int	ts_swapxy;
	int	ts_resx;
	int	ts_resy;
} uts_scale = {
	3800, 500, 450, 3800, 1, 1024, 768
};

struct uts_pos {
	int	x;
	int	y;
	int	z;	/* touch pressure */
};

Static const struct usb_devno uts_devs[] = {
	{ USB_VENDOR_FTDI,		USB_PRODUCT_FTDI_ITM_TOUCH },
	{ USB_VENDOR_EGALAX,		USB_PRODUCT_EGALAX_TPANEL },
	{ USB_VENDOR_EGALAX,		USB_PRODUCT_EGALAX_TPANEL2 },
	{ 0, 0 }
};

Static void uts_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
struct uts_pos uts_get_pos(usbd_private_handle addr, struct uts_pos tp);

Static int	uts_enable(void *);
Static void	uts_disable(void *);
Static int	uts_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wsmouse_accessops uts_accessops = {
	uts_enable,
	uts_ioctl,
	uts_disable,
};

USB_DECLARE_DRIVER(uts);

USB_MATCH(uts)
{
	USB_MATCH_START(uts, uaa);

	if (uaa->iface == NULL)
		return UMATCH_NONE;

	return (usb_lookup(uts_devs, uaa->vendor, uaa->product) != NULL) ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

USB_ATTACH(uts)
{
	USB_ATTACH_START(uts, sc, uaa);
	usb_config_descriptor_t *cdesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct wsmousedev_attach_args a;
	char *devinfop;
	int i, found;

	sc->sc_udev = uaa->device;
	sc->sc_product = uaa->product;
	sc->sc_intr_number = -1;
	sc->sc_intr_pipe = NULL;
	sc->sc_enabled = sc->sc_isize = 0;

	/* Display device info string */
	USB_ATTACH_SETUP;
	if ((devinfop = usbd_devinfo_alloc(uaa->device, 0)) != NULL) {
		printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
		usbd_devinfo_free(devinfop);
	}

	/* Move the device into the configured state. */
	if (usbd_set_config_index(uaa->device, UTS_CONFIG_INDEX, 1) != 0) {
		printf("%s: could not set configuartion no\n",
			USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}

	/* get the config descriptor */
	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL) {
		printf("%s: failed to get configuration descriptor\n",
			USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}

	/* get the interface */
	if (usbd_device2interface_handle(uaa->device, 0, &sc->sc_iface) != 0) {
		printf("%s: failed to get interface\n",
			USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}

	/* Find the interrupt endpoint */
	id = usbd_get_interface_descriptor(sc->sc_iface);
	sc->sc_iface_number = id->bInterfaceNumber;
	found = 0;

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for %d\n",
				USBDEVNAME(sc->sc_dev), i);
			sc->sc_dying = 1;
			USB_ATTACH_ERROR_RETURN;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_intr_number = ed->bEndpointAddress;
			sc->sc_isize = UGETW(ed->wMaxPacketSize);
		}
	}

	if (sc->sc_intr_number== -1) {
		printf("%s: Could not find interrupt in\n",
			USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	a.accessops = &uts_accessops;
	a.accesscookie = sc;

	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(uts)
{
	USB_DETACH_START(uts, sc);
	int rv = 0;

	if (sc->sc_intr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_intr_pipe);
		usbd_close_pipe(sc->sc_intr_pipe);
		sc->sc_intr_pipe = NULL;
	}

	sc->sc_dying = 1;

	if (sc->sc_wsmousedev != NULL) {
		rv = config_detach(sc->sc_wsmousedev, flags);
		sc->sc_wsmousedev = NULL;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (rv);
}

int
uts_activate(device_ptr_t self, enum devact act)
{
	struct uts_softc *sc = (struct uts_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_wsmousedev != NULL)
			rv = config_deactivate(sc->sc_wsmousedev);
		sc->sc_dying = 1;
		break;
	}

	return (rv);
}

Static int
uts_enable(void *v)
{
	struct uts_softc *sc = v;
	int err;

	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_enabled)
		return (EBUSY);

	if (sc->sc_isize == 0)
		return 0;
	sc->sc_ibuf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
	err = usbd_open_pipe_intr(sc->sc_iface, sc->sc_intr_number,
		USBD_SHORT_XFER_OK, &sc->sc_intr_pipe, sc, sc->sc_ibuf,
		sc->sc_isize, uts_intr, USBD_DEFAULT_INTERVAL);
	if (err) {
		free(sc->sc_ibuf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
		return EIO;
	}

	sc->sc_enabled = 1;
	sc->sc_buttons = 0;

	return (0);
}

Static void
uts_disable(void *v)
{
	struct uts_softc *sc = v;

	if (!sc->sc_enabled) {
		printf("uts_disable: already disabled!\n");
		return;
	}

	/* Disable interrupts. */
	if (sc->sc_intr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_intr_pipe);
		usbd_close_pipe(sc->sc_intr_pipe);
		sc->sc_intr_pipe = NULL;
	}

	if (sc->sc_ibuf != NULL) {
		free(sc->sc_ibuf, M_USBDEV);
		sc->sc_ibuf = NULL;
	}

	sc->sc_enabled = 0;
}

Static int
uts_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *l)
{
	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_TPANEL;
		return (0);
	}

	return (-1);
}

struct uts_pos
uts_get_pos(usbd_private_handle addr, struct uts_pos tp)
{
	struct uts_softc *sc = addr;
	struct utsscale *tsp = &uts_scale;
	u_char *p = sc->sc_ibuf;
	int down, x, y;

	switch (sc->sc_product) {
	case USB_PRODUCT_FTDI_ITM_TOUCH:
		down = (p[7] & 0x20);	
		x = ((p[0] & 0x1f) << 7) | (p[3] & 0x7f);  
		y = ((p[1] & 0x1f) << 7) | (p[4] & 0x7f);
		break;
	case USB_PRODUCT_EGALAX_TPANEL:
	case USB_PRODUCT_EGALAX_TPANEL2:
		down = (p[0] & 0x01);
		x = ((p[3] & 0x0f) << 7) | (p[4] & 0x7f);
		y = ((p[1] & 0x0f) << 7) | (p[2] & 0x7f);
		break;
	}

	if (!down) {
		if (tsp->ts_swapxy) {	/* Swap X/Y-Axis */
			tp.y = x;
			tp.x = y;
		} else {
			tp.x = x;
			tp.y = y;
		}
	
		if (!uts_rawmode) {
			/* Scale down to the screen resolution. */
			tp.x = ((tp.x - tsp->ts_minx) * tsp->ts_resx) /
			    (tsp->ts_maxx - tsp->ts_minx);
			tp.y = ((tp.y - tsp->ts_miny) * tsp->ts_resy) /
			    (tsp->ts_maxy - tsp->ts_miny);
		}
		tp.z = 1;
	} else {
		/* x/y values are not reliable if there is no pressure */
		tp.x = sc->sc_oldx;
		tp.y = sc->sc_oldy;
		tp.z = 0;
	}

	return (tp);
}

Static void
uts_intr(usbd_xfer_handle xfer, usbd_private_handle addr, usbd_status status)
{
	struct uts_softc *sc = addr;
	u_int32_t len;
	int s;
	struct uts_pos tp;

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	s = spltty();

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		printf("%s: status %d\n", USBDEVNAME(sc->sc_dev), status);
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	if (len != 8) {
		printf("%s: bad input length %d != %d\n",
			USBDEVNAME(sc->sc_dev), len, sc->sc_isize);
		return;
	}

	tp = uts_get_pos(sc, tp);

	wsmouse_input(sc->sc_wsmousedev, tp.z, tp.x, tp.y, 0,
		WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y |
		WSMOUSE_INPUT_ABSOLUTE_Z); 
	sc->sc_oldy = tp.y;
	sc->sc_oldx = tp.x;

	splx(s);
}
