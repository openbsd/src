/*	$OpenBSD: ums.c,v 1.31 2009/10/13 19:33:19 pirofti Exp $ */
/*	$NetBSD: ums.c,v 1.60 2003/03/11 16:44:00 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	do { if (umsdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (umsdebug>(n)) printf x; } while (0)
int	umsdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UMS_BUT(i) ((i) == 1 || (i) == 2 ? 3 - (i) : i)

#define UMSUNIT(s)	(minor(s))

#define MAX_BUTTONS	16	/* must not exceed size of sc_buttons */

struct ums_softc {
	struct uhidev sc_hdev;

	struct hid_location sc_loc_x, sc_loc_y, sc_loc_z, sc_loc_w;
	struct hid_location sc_loc_btn[MAX_BUTTONS];

	int sc_enabled;

	int flags;		/* device configuration */
#define UMS_Z		0x01	/* Z direction available */
#define UMS_SPUR_BUT_UP	0x02	/* spurious button up events */
#define UMS_REVZ	0x04	/* Z-axis is reversed */
#define UMS_W		0x08	/* W direction available */
#define UMS_REVW	0x10	/* W-axis is reversed */
#define UMS_LEADINGBYTE	0x20	/* Unknown leading byte */

	int nbuttons;

	u_int32_t sc_buttons;	/* mouse button status */
	struct device *sc_wsmousedev;

	char			sc_dying;
};

#define MOUSE_FLAGS_MASK (HIO_CONST|HIO_RELATIVE)
#define MOUSE_FLAGS (HIO_RELATIVE)

void ums_intr(struct uhidev *addr, void *ibuf, u_int len);

int	ums_enable(void *);
void	ums_disable(void *);
int	ums_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wsmouse_accessops ums_accessops = {
	ums_enable,
	ums_ioctl,
	ums_disable,
};

int ums_match(struct device *, void *, void *); 
void ums_attach(struct device *, struct device *, void *); 
int ums_detach(struct device *, int); 
int ums_activate(struct device *, int); 

struct cfdriver ums_cd = { 
	NULL, "ums", DV_DULL 
}; 

const struct cfattach ums_ca = { 
	sizeof(struct ums_softc), 
	ums_match, 
	ums_attach, 
	ums_detach, 
	ums_activate, 
};

int
ums_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	int size;
	void *desc;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	if (!hid_is_collection(desc, size, uha->reportid,
			       HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_MOUSE)))
		return (UMATCH_NONE);

	return (UMATCH_IFACECLASS);
}

void
ums_attach(struct device *parent, struct device *self, void *aux)
{
	struct ums_softc *sc = (struct ums_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	struct wsmousedev_attach_args a;
	int size;
	void *desc;
	u_int32_t flags, quirks;
	int i, wheel, twheel;

	sc->sc_hdev.sc_intr = ums_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	quirks = usbd_get_quirks(uha->parent->sc_udev)->uq_flags;
	if (quirks & UQ_MS_REVZ)
		sc->flags |= UMS_REVZ;
	if (quirks & UQ_SPUR_BUT_UP)
		sc->flags |= UMS_SPUR_BUT_UP;
	if (quirks & UQ_MS_LEADING_BYTE)
		sc->flags |= UMS_LEADINGBYTE;

	uhidev_get_report_desc(uha->parent, &desc, &size);

	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
	       uha->reportid, hid_input, &sc->sc_loc_x, &flags)) {
		printf("\n%s: mouse has no X report\n",
		       sc->sc_hdev.sc_dev.dv_xname);
		return;
	}
	if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
		printf("\n%s: X report 0x%04x not supported\n",
		       sc->sc_hdev.sc_dev.dv_xname, flags);
		return;
	}

	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
	       uha->reportid, hid_input, &sc->sc_loc_y, &flags)) {
		printf("\n%s: mouse has no Y report\n",
		       sc->sc_hdev.sc_dev.dv_xname);
		return;
	}
	if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
		printf("\n%s: Y report 0x%04x not supported\n",
		       sc->sc_hdev.sc_dev.dv_xname, flags);
		return;
	}

	/*
	 * Try to guess the Z activator: check WHEEL, TWHEEL, and Z,
	 * in that order.
	 */

	wheel = hid_locate(desc, size,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_WHEEL),
	    uha->reportid, hid_input, &sc->sc_loc_z, &flags);
	if (wheel == 0)
		twheel = hid_locate(desc, size,
		    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_TWHEEL),
		    uha->reportid, hid_input, &sc->sc_loc_z, &flags);
	else
		twheel = 0;

	if (wheel || twheel) {
		if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
			DPRINTF(("\n%s: Wheel report 0x%04x not supported\n",
				sc->sc_hdev.sc_dev.dv_xname, flags));
			sc->sc_loc_z.size = 0; /* Bad Z coord, ignore it */
		} else {
			sc->flags |= UMS_Z;
			/* Wheels need the Z axis reversed. */
			sc->flags ^= UMS_REVZ;
		}
		/*
		 * We might have both a wheel and Z direction; in this case,
		 * report the Z direction on the W axis.
		*/
		if (hid_locate(desc, size,
		    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z),
		    uha->reportid, hid_input, &sc->sc_loc_w, &flags)) {
			if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
				DPRINTF(("\n%s: Z report 0x%04x not supported\n",
					sc->sc_hdev.sc_dev.dv_xname, flags));
				/* Bad Z coord, ignore it */
				sc->sc_loc_w.size = 0;
			}
			else
				sc->flags |= UMS_W;
		}
	} else if (hid_locate(desc, size,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z),
	    uha->reportid, hid_input, &sc->sc_loc_z, &flags)) {
		if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
			DPRINTF(("\n%s: Z report 0x%04x not supported\n",
				sc->sc_hdev.sc_dev.dv_xname, flags));
			sc->sc_loc_z.size = 0; /* Bad Z coord, ignore it */
		} else {
			sc->flags |= UMS_Z;
		}
	}

	/*
	 * The Microsoft Wireless Intellimouse 2.0 reports its wheel
	 * using 0x0048 (I've called it HUG_TWHEEL) and seems to expect
	 * us to know that the byte after the wheel is the tilt axis.
	 * There are no other HID axis descriptors other than X, Y and
	 * TWHEEL, so we report TWHEEL on the W axis.
	 */
	if (twheel) {
		sc->sc_loc_w = sc->sc_loc_z;
		sc->sc_loc_w.pos = sc->sc_loc_w.pos + 8;
		sc->flags |= UMS_W | UMS_LEADINGBYTE;
		/* Wheels need their axis reversed. */
		sc->flags ^= UMS_REVW;
	}

	/* figure out the number of buttons */
	for (i = 1; i <= MAX_BUTTONS; i++)
		if (!hid_locate(desc, size, HID_USAGE2(HUP_BUTTON, i),
		    uha->reportid, hid_input, &sc->sc_loc_btn[i - 1], 0))
			break;
	sc->nbuttons = i - 1;

	/*
	 * The Microsoft Wireless Notebook Optical Mouse seems to be in worse
	 * shape than the Wireless Intellimouse 2.0, as its X, Y, wheel, and
	 * all of its other button positions are all off. It also reports that
	 * it has two addional buttons and a tilt wheel.
	 */
	if (quirks & UQ_MS_BAD_CLASS) {
		/* UMS_LEADINGBYTE cleared on purpose */
		sc->flags = UMS_Z | UMS_SPUR_BUT_UP;
		sc->nbuttons = 3;
		/* XXX change sc_hdev isize to 5? */
		/* 1st byte of descriptor report contains garbage */
		sc->sc_loc_x.pos = 16;
		sc->sc_loc_y.pos = 24;
		sc->sc_loc_z.pos = 32;
		sc->sc_loc_btn[0].pos = 8;
		sc->sc_loc_btn[1].pos = 9;
		sc->sc_loc_btn[2].pos = 10;
	}

	/*
	 * The Microsoft Wireless Notebook Optical Mouse 3000 Model 1049 has
	 * five Report IDs: 19, 23, 24, 17, 18 (in the order they appear in
	 * report descriptor), it seems that report 17 contains the necessary
	 * mouse information (3-buttons, X, Y, wheel) so we specify it
	 * manually.
	 */
	if (uaa->vendor == USB_VENDOR_MICROSOFT &&
	    uaa->product == USB_PRODUCT_MICROSOFT_WLNOTEBOOK3) {
		sc->flags = UMS_Z;
		sc->nbuttons = 3;
		/* XXX change sc_hdev isize to 5? */
		sc->sc_loc_x.pos = 8;
		sc->sc_loc_y.pos = 16;
		sc->sc_loc_z.pos = 24;
		sc->sc_loc_btn[0].pos = 0;
		sc->sc_loc_btn[1].pos = 1;
		sc->sc_loc_btn[2].pos = 2;
	}

	printf(": %d button%s",
	    sc->nbuttons, sc->nbuttons <= 1 ? "" : "s");
	switch (sc->flags & (UMS_Z | UMS_W)) {
	case UMS_Z:
		printf(", Z dir");
		break;
	case UMS_W:
		printf(", W dir");
		break;
	case UMS_Z | UMS_W:
		printf(", Z and W dir");
		break;
	}
	printf("\n");

#ifdef USB_DEBUG
	DPRINTF(("ums_attach: sc=%p\n", sc));
	DPRINTF(("ums_attach: X\t%d/%d\n",
	     sc->sc_loc_x.pos, sc->sc_loc_x.size));
	DPRINTF(("ums_attach: Y\t%d/%d\n",
	    sc->sc_loc_y.pos, sc->sc_loc_y.size));
	if (sc->flags & UMS_Z)
		DPRINTF(("ums_attach: Z\t%d/%d\n",
		    sc->sc_loc_z.pos, sc->sc_loc_z.size));
	if (sc->flags & UMS_W)
		DPRINTF(("ums_attach: W\t%d/%d\n",
		    sc->sc_loc_w.pos, sc->sc_loc_w.size));
	for (i = 1; i <= sc->nbuttons; i++) {
		DPRINTF(("ums_attach: B%d\t%d/%d\n",
		    i, sc->sc_loc_btn[i - 1].pos, sc->sc_loc_btn[i - 1].size));
	}
#endif

	a.accessops = &ums_accessops;
	a.accesscookie = sc;

	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}

int
ums_activate(struct device *self, int act)
{
	struct ums_softc *sc = (struct ums_softc *)self;
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

int
ums_detach(struct device *self, int flags)
{
	struct ums_softc *sc = (struct ums_softc *)self;
	int rv = 0;

	DPRINTF(("ums_detach: sc=%p flags=%d\n", sc, flags));

	/* No need to do reference counting of ums, wsmouse has all the goo. */
	if (sc->sc_wsmousedev != NULL)
		rv = config_detach(sc->sc_wsmousedev, flags);

	return (rv);
}

void
ums_intr(struct uhidev *addr, void *buf, u_int len)
{
	struct ums_softc *sc = (struct ums_softc *)addr;
	u_char *ibuf = (u_char *)buf;
	int dx, dy, dz, dw;
	u_int32_t buttons = 0;
	int i;
	int s;

	DPRINTFN(5,("ums_intr: len=%d\n", len));

	/*
	 * The Microsoft Wireless Intellimouse 2.0 sends one extra leading
	 * byte of data compared to most USB mice.  This byte frequently
	 * switches from 0x01 (usual state) to 0x02.  It may be used to
	 * report non-standard events (such as battery life).  However,
	 * at the same time, it generates a left click event on the
	 * button byte, where there shouldn't be any.  We simply discard
	 * the packet in this case.
	 *
	 * This problem affects the MS Wireless Notebook Optical Mouse, too.
	 * However, the leading byte for this mouse is normally 0x11, and
	 * the phantom mouse click occurs when it's 0x14.
	 */
	if (sc->flags & UMS_LEADINGBYTE) {
		if (*ibuf++ == 0x02)
			return;
		/* len--; */
	} else if (sc->flags & UMS_SPUR_BUT_UP) {
		if (*ibuf == 0x14 || *ibuf == 0x15)
			return;
	}

	dx =  hid_get_data(ibuf, &sc->sc_loc_x);
	dy = -hid_get_data(ibuf, &sc->sc_loc_y);
	dz =  hid_get_data(ibuf, &sc->sc_loc_z);
	dw =  hid_get_data(ibuf, &sc->sc_loc_w);
	if (sc->flags & UMS_REVZ)
		dz = -dz;
	if (sc->flags & UMS_REVW)
		dw = -dw;
	for (i = 0; i < sc->nbuttons; i++)
		if (hid_get_data(ibuf, &sc->sc_loc_btn[i]))
			buttons |= (1 << UMS_BUT(i));

	if (dx != 0 || dy != 0 || dz != 0 || dw != 0 ||
	    buttons != sc->sc_buttons) {
		DPRINTFN(10, ("ums_intr: x:%d y:%d z:%d w:%d buttons:0x%x\n",
			dx, dy, dz, dw, buttons));
		sc->sc_buttons = buttons;
		if (sc->sc_wsmousedev != NULL) {
			s = spltty();
			wsmouse_input(sc->sc_wsmousedev, buttons,
			    dx, dy, dz, dw, WSMOUSE_INPUT_DELTA);
			splx(s);
		}
	}
}

int
ums_enable(void *v)
{
	struct ums_softc *sc = v;

	DPRINTFN(1,("ums_enable: sc=%p\n", sc));

	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_enabled)
		return (EBUSY);

	sc->sc_enabled = 1;
	sc->sc_buttons = 0;

	return (uhidev_open(&sc->sc_hdev));
}

void
ums_disable(void *v)
{
	struct ums_softc *sc = v;

	DPRINTFN(1,("ums_disable: sc=%p\n", sc));
#ifdef DIAGNOSTIC
	if (!sc->sc_enabled) {
		printf("ums_disable: not enabled\n");
		return;
	}
#endif

	sc->sc_enabled = 0;
	uhidev_close(&sc->sc_hdev);
}

int
ums_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)

{
	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_USB;
		return (0);
	}

	return (-1);
}
