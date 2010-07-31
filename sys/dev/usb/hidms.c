/*	$OpenBSD: hidms.c,v 1.1 2010/07/31 16:04:50 miod Exp $ */
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

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usb_quirks.h>
#include <dev/usb/hid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/usb/hidmsvar.h>

#ifdef HIDMS_DEBUG
#define DPRINTF(x)	do { if (hidmsdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (hidmsdebug>(n)) printf x; } while (0)
int	hidmsdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define HIDMS_BUT(i)	((i) == 1 || (i) == 2 ? 3 - (i) : i)

#define NOTMOUSE(f)	(((f) & (HIO_CONST | HIO_RELATIVE)) != HIO_RELATIVE)

int
hidms_setup(struct device *self, struct hidms *ms, uint32_t quirks,
    int id, void *desc, int dlen)
{
	uint32_t flags;
	int i, wheel, twheel;

	ms->sc_device = self;

	if (quirks & UQ_MS_REVZ)
		ms->sc_flags |= HIDMS_REVZ;
	if (quirks & UQ_SPUR_BUT_UP)
		ms->sc_flags |= HIDMS_SPUR_BUT_UP;
	if (quirks & UQ_MS_LEADING_BYTE)
		ms->sc_flags |= HIDMS_LEADINGBYTE;

	if (!hid_locate(desc, dlen, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X), id,
	    hid_input, &ms->sc_loc_x, &flags)) {
		printf("\n%s: mouse has no X report\n", self->dv_xname);
		return ENXIO;
	}
	if (NOTMOUSE(flags)) {
		printf("\n%s: X report 0x%04x not supported\n",
		    self->dv_xname, flags);
		return ENXIO;
	}

	if (!hid_locate(desc, dlen, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y), id,
	    hid_input, &ms->sc_loc_y, &flags)) {
		printf("\n%s: mouse has no Y report\n", self->dv_xname);
		return ENXIO;
	}
	if (NOTMOUSE(flags)) {
		printf("\n%s: Y report 0x%04x not supported\n",
		    self->dv_xname, flags);
		return ENXIO;
	}

	/*
	 * Try to guess the Z activator: check WHEEL, TWHEEL, and Z,
	 * in that order.
	 */

	wheel = hid_locate(desc, dlen,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_WHEEL), id,
	    hid_input, &ms->sc_loc_z, &flags);
	if (wheel == 0)
		twheel = hid_locate(desc, dlen,
		    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_TWHEEL), id,
		    hid_input, &ms->sc_loc_z, &flags);
	else
		twheel = 0;

	if (wheel || twheel) {
		if (NOTMOUSE(flags)) {
			DPRINTF(("\n%s: Wheel report 0x%04x not supported\n",
			    self->dv_xname, flags));
			ms->sc_loc_z.size = 0; /* Bad Z coord, ignore it */
		} else {
			ms->sc_flags |= HIDMS_Z;
			/* Wheels need the Z axis reversed. */
			ms->sc_flags ^= HIDMS_REVZ;
		}
		/*
		 * We might have both a wheel and Z direction; in this case,
		 * report the Z direction on the W axis.
		*/
		if (hid_locate(desc, dlen,
		    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z), id,
		    hid_input, &ms->sc_loc_w, &flags)) {
			if (NOTMOUSE(flags)) {
				DPRINTF(("\n%s: Z report 0x%04x not supported\n",
				    self->dv_xname, flags));
				/* Bad Z coord, ignore it */
				ms->sc_loc_w.size = 0;
			}
			else
				ms->sc_flags |= HIDMS_W;
		}
	} else if (hid_locate(desc, dlen,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z), id,
	    hid_input, &ms->sc_loc_z, &flags)) {
		if (NOTMOUSE(flags)) {
			DPRINTF(("\n%s: Z report 0x%04x not supported\n",
			    self->dv_xname, flags));
			ms->sc_loc_z.size = 0; /* Bad Z coord, ignore it */
		} else {
			ms->sc_flags |= HIDMS_Z;
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
		ms->sc_loc_w = ms->sc_loc_z;
		ms->sc_loc_w.pos = ms->sc_loc_w.pos + 8;
		ms->sc_flags |= HIDMS_W | HIDMS_LEADINGBYTE;
		/* Wheels need their axis reversed. */
		ms->sc_flags ^= HIDMS_REVW;
	}

	/* figure out the number of buttons */
	for (i = 1; i <= MAX_BUTTONS; i++)
		if (!hid_locate(desc, dlen, HID_USAGE2(HUP_BUTTON, i), id,
		    hid_input, &ms->sc_loc_btn[i - 1], 0))
			break;
	ms->sc_num_buttons = i - 1;

	/*
	 * The Microsoft Wireless Notebook Optical Mouse seems to be in worse
	 * shape than the Wireless Intellimouse 2.0, as its X, Y, wheel, and
	 * all of its other button positions are all off. It also reports that
	 * it has two addional buttons and a tilt wheel.
	 */
	if (quirks & UQ_MS_BAD_CLASS) {
		/* HIDMS_LEADINGBYTE cleared on purpose */
		ms->sc_flags = HIDMS_Z | HIDMS_SPUR_BUT_UP;
		ms->sc_num_buttons = 3;
		/* XXX change sc_hdev isize to 5? */
		/* 1st byte of descriptor report contains garbage */
		ms->sc_loc_x.pos = 16;
		ms->sc_loc_y.pos = 24;
		ms->sc_loc_z.pos = 32;
		ms->sc_loc_btn[0].pos = 8;
		ms->sc_loc_btn[1].pos = 9;
		ms->sc_loc_btn[2].pos = 10;
	}

	return 0;
}

void
hidms_attach(struct hidms *ms, const struct wsmouse_accessops *ops)
{
	struct wsmousedev_attach_args a;
#ifdef HIDMS_DEBUG
	int i;
#endif

	printf(": %d button%s",
	    ms->sc_num_buttons, ms->sc_num_buttons <= 1 ? "" : "s");
	switch (ms->sc_flags & (HIDMS_Z | HIDMS_W)) {
	case HIDMS_Z:
		printf(", Z dir");
		break;
	case HIDMS_W:
		printf(", W dir");
		break;
	case HIDMS_Z | HIDMS_W:
		printf(", Z and W dir");
		break;
	}
	printf("\n");

#ifdef HIDMS_DEBUG
	DPRINTF(("hidms_attach: sc=%p\n", sc));
	DPRINTF(("hidms_attach: X\t%d/%d\n",
	     ms->sc_loc_x.pos, ms->sc_loc_x.size));
	DPRINTF(("hidms_attach: Y\t%d/%d\n",
	    ms->sc_loc_y.pos, ms->sc_loc_y.size));
	if (ms->sc_flags & HIDMS_Z)
		DPRINTF(("hidms_attach: Z\t%d/%d\n",
		    ms->sc_loc_z.pos, ms->sc_loc_z.size));
	if (ms->sc_flags & HIDMS_W)
		DPRINTF(("hidms_attach: W\t%d/%d\n",
		    ms->sc_loc_w.pos, ms->sc_loc_w.size));
	for (i = 1; i <= ms->sc_num_buttons; i++) {
		DPRINTF(("hidms_attach: B%d\t%d/%d\n",
		    i, ms->sc_loc_btn[i - 1].pos, ms->sc_loc_btn[i - 1].size));
	}
#endif

	a.accessops = ops;
	a.accesscookie = ms->sc_device;
	ms->sc_wsmousedev = config_found(ms->sc_device, &a, wsmousedevprint);
}

int
hidms_detach(struct hidms *ms, int flags)
{
	int rv = 0;

	DPRINTF(("hidms_detach: sc=%p flags=%d\n", sc, flags));

	/* No need to do reference counting of hidms, wsmouse has all the goo */
	if (ms->sc_wsmousedev != NULL)
		rv = config_detach(ms->sc_wsmousedev, flags);

	return (rv);
}

void
hidms_input(struct hidms *ms, uint8_t *data, u_int len)
{
	int dx, dy, dz, dw;
	u_int32_t buttons = 0;
	int i, s;

	DPRINTFN(5,("hidms_input: len=%d\n", len));

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
	if (ms->sc_flags & HIDMS_LEADINGBYTE) {
		if (*data++ == 0x02)
			return;
		/* len--; */
	} else if (ms->sc_flags & HIDMS_SPUR_BUT_UP) {
		if (*data == 0x14 || *data == 0x15)
			return;
	}

	dx =  hid_get_data(data, &ms->sc_loc_x);
	dy = -hid_get_data(data, &ms->sc_loc_y);
	dz =  hid_get_data(data, &ms->sc_loc_z);
	dw =  hid_get_data(data, &ms->sc_loc_w);

	if (ms->sc_flags & HIDMS_REVZ)
		dz = -dz;
	if (ms->sc_flags & HIDMS_REVW)
		dw = -dw;

	for (i = 0; i < ms->sc_num_buttons; i++)
		if (hid_get_data(data, &ms->sc_loc_btn[i]))
			buttons |= (1 << HIDMS_BUT(i));

	if (dx != 0 || dy != 0 || dz != 0 || dw != 0 ||
	    buttons != ms->sc_buttons) {
		DPRINTFN(10, ("hidms_input: x:%d y:%d z:%d w:%d buttons:0x%x\n",
			dx, dy, dz, dw, buttons));
		ms->sc_buttons = buttons;
		if (ms->sc_wsmousedev != NULL) {
			s = spltty();
			wsmouse_input(ms->sc_wsmousedev, buttons,
			    dx, dy, dz, dw, WSMOUSE_INPUT_DELTA);
			splx(s);
		}
	}
}

int
hidms_enable(struct hidms *ms)
{
	if (ms->sc_enabled)
		return EBUSY;

	ms->sc_enabled = 1;
	ms->sc_buttons = 0;
	return 0;
}

int
hidms_ioctl(struct hidms *ms, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	switch (cmd) {
	default:
		return -1;
	}
}

void
hidms_disable(struct hidms *ms)
{
	ms->sc_enabled = 0;
}
