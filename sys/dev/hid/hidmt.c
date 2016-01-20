/* $OpenBSD: hidmt.c,v 1.1 2016/01/20 01:26:00 jcs Exp $ */
/*
 * HID multitouch driver for devices conforming to Windows Precision Touchpad
 * standard
 *
 * https://msdn.microsoft.com/en-us/library/windows/hardware/dn467314%28v=vs.85%29.aspx
 *
 * Copyright (c) 2016 joshua stein <jcs@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidmtvar.h>

/* #define HIDMT_DEBUG */

#ifdef HIDMT_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

int
hidmt_setup(struct device *self, struct hidmt *mt, void *desc, int dlen)
{
	struct hid_location cap;
	int32_t d;
	uint8_t *rep;
	int capsize;

	struct hid_data *hd;
	struct hid_item h;

	mt->sc_device = self;
	mt->sc_rep_input_size = hid_report_size(desc, dlen, hid_input,
	    mt->sc_rep_input);

	mt->sc_minx = mt->sc_miny = mt->sc_maxx = mt->sc_maxy = 0;

	capsize = hid_report_size(desc, dlen, hid_feature, mt->sc_rep_cap);
	rep = malloc(capsize, M_DEVBUF, M_NOWAIT | M_ZERO);

	if (mt->hidev_get_report(mt->sc_device, hid_feature, mt->sc_rep_cap,
	    rep, capsize)) {
		printf("\n%s: failed getting capability report\n",
		    self->dv_xname);
		return 1;
	}

	/* find maximum number of contacts being reported per input report */
	if (!hid_locate(desc, dlen, HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACT_MAX),
	    mt->sc_rep_cap, hid_feature, &cap, NULL)) {
		printf("\n%s: can't find maximum contacts\n", self->dv_xname);
		return 1;
	}

	d = hid_get_udata(rep, capsize, &cap);
	if (d > HIDMT_MAX_CONTACTS) {
		printf("\n%s: contacts %d > max %d\n", self->dv_xname, d,
		    HIDMT_MAX_CONTACTS);
		return 1;
	}
	else
		mt->sc_num_contacts = d;

	/* find whether this is a clickpad or not */
	if (!hid_locate(desc, dlen, HID_USAGE2(HUP_DIGITIZERS, HUD_BUTTON_TYPE),
	    mt->sc_rep_cap, hid_feature, &cap, NULL)) {
		printf("\n%s: can't find button type\n", self->dv_xname);
		return 1;
	}

	d = hid_get_udata(rep, capsize, &cap);
	mt->sc_clickpad = (d == 0);

	/*
	 * Walk HID descriptor and store usages we care about to know what to
	 * pluck out of input reports.
	 */

	SIMPLEQ_INIT(&mt->sc_inputs);

	hd = hid_start_parse(desc, dlen, hid_input);
	while (hid_get_item(hd, &h)) {
		struct hidmt_input *input;

		if (h.report_ID != mt->sc_rep_input)
			continue;

		switch (h.usage) {
		/* contact level usages */
		case HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X):
			if (h.physical_minimum < mt->sc_minx)
				mt->sc_minx = h.physical_minimum;
			if (h.physical_maximum > mt->sc_maxx)
				mt->sc_maxx = h.physical_maximum;

			break;
		case HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y):
			if (h.physical_minimum < mt->sc_miny)
				mt->sc_miny = h.physical_minimum;
			if (h.physical_maximum > mt->sc_maxy)
				mt->sc_maxy = h.physical_maximum;

			break;
		case HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_SWITCH):
		case HID_USAGE2(HUP_DIGITIZERS, HUD_CONFIDENCE):
		case HID_USAGE2(HUP_DIGITIZERS, HUD_WIDTH):
		case HID_USAGE2(HUP_DIGITIZERS, HUD_HEIGHT):
		case HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACTID):

		/* report level usages */
		case HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACTCOUNT):
		case HID_USAGE2(HUP_BUTTON, 0x01):
			break;
		default:
			continue;
		}

		input = malloc(sizeof(*input), M_DEVBUF, M_NOWAIT | M_ZERO);
		memcpy(&input->loc, &h.loc, sizeof(struct hid_location));
		input->usage = h.usage;

		SIMPLEQ_INSERT_TAIL(&mt->sc_inputs, input, entry);
	}
	hid_end_parse(hd);

	if (mt->sc_maxx <= 0 || mt->sc_maxy <= 0) {
		printf("\n%s: invalid max X/Y %d/%d\n", self->dv_xname,
		    mt->sc_maxx, mt->sc_maxy);
		return 1;
	}

	if (hidmt_set_input_mode(mt, HIDMT_INPUT_MODE_MT)) {
		printf("\n%s: switch to multitouch mode failed\n",
		    self->dv_xname);
		return 1;
	}

	return 0;
}

void
hidmt_attach(struct hidmt *mt, const struct wsmouse_accessops *ops)
{
	struct wsmousedev_attach_args a;

	printf(": %spad, %d contact%s\n",
	    (mt->sc_clickpad ? "click" : "touch"), mt->sc_num_contacts,
	    (mt->sc_num_contacts == 1 ? "" : "s"));

	a.accessops = ops;
	a.accesscookie = mt->sc_device;
	mt->sc_wsmousedev = config_found(mt->sc_device, &a, wsmousedevprint);
}

int
hidmt_detach(struct hidmt *mt, int flags)
{
	int rv = 0;

	if (mt->sc_wsmousedev != NULL)
		rv = config_detach(mt->sc_wsmousedev, flags);

	return (rv);
}

int
hidmt_set_input_mode(struct hidmt *mt, int mode)
{
	return mt->hidev_set_report(mt->sc_device, hid_feature,
	    mt->sc_rep_config, &mode, 1);
}

void
hidmt_input(struct hidmt *mt, uint8_t *data, u_int len)
{
	struct hidmt_input *hi;
	struct hidmt_contact hc;
	int32_t d, firstu = 0;
	int contactcount = 0, seencontacts = 0, tips = 0, i, s;

	if (len != mt->sc_rep_input_size) {
		DPRINTF(("%s: %s: length %d not %d, ignoring\n",
		    mt->sc_device->dv_xname, __func__, len,
		    mt->sc_rep_input_size));
		return;
	}

	/*
	 * "In Parallel mode, devices report all contact information in a
	 * single packet. Each physical contact is represented by a logical
	 * collection that is embedded in the top-level collection."
	 *
	 * Since additional contacts that were not present will still be in the
	 * report with contactid=0 but contactids are zero-based, find
	 * contactcount first.
	 */
	SIMPLEQ_FOREACH(hi, &mt->sc_inputs, entry) {
		if (hi->usage == HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACTCOUNT))
			contactcount = hid_get_udata(data, len,
			    &hi->loc);
	}

	if (!contactcount) {
		DPRINTF(("%s: %s: no contactcount in report\n",
		    mt->sc_device->dv_xname, __func__));
		return;
	}

	/*
	 * Walk through each input we know about and fetch its data from the
	 * report, storing it in a temporary contact.  Once we see our first
	 * usage again, we'll know we saw all usages being presented for that
	 * contact.
	 */
	bzero(&hc, sizeof(struct hidmt_contact));
	SIMPLEQ_FOREACH(hi, &mt->sc_inputs, entry) {
		d = hid_get_udata(data, len, &hi->loc);

		if (firstu && hi->usage == firstu) {
			if (seencontacts < contactcount &&
			    hc.contactid < HIDMT_MAX_CONTACTS) {
				hc.seen = 1;
				memcpy(&mt->sc_contacts[hc.contactid], &hc,
				    sizeof(struct hidmt_contact));
				seencontacts++;
			}

			bzero(&hc, sizeof(struct hidmt_contact));
		}
		else if (!firstu)
			firstu = hi->usage;

		switch (hi->usage) {
		case HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X):
			hc.x = d;
			break;
		case HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y):
			if (mt->sc_flags & HIDMT_REVY)
				hc.y = mt->sc_maxy - d;
			else
				hc.y = d;
			break;
		case HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_SWITCH):
			hc.tip = d;
			if (d)
				tips++;
			break;
		case HID_USAGE2(HUP_DIGITIZERS, HUD_CONFIDENCE):
			hc.confidence = d;
			break;
		case HID_USAGE2(HUP_DIGITIZERS, HUD_WIDTH):
			hc.width = d;
			break;
		case HID_USAGE2(HUP_DIGITIZERS, HUD_HEIGHT):
			hc.height = d;
			break;
		case HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACTID):
			hc.contactid = d;
			break;

		/* these will only appear once per report */
		case HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACTCOUNT):
			contactcount = d;
			break;
		case HID_USAGE2(HUP_BUTTON, 0x01):
			mt->sc_button = (d != 0);
			break;
		}
	}
	if (seencontacts < contactcount && hc.contactid < HIDMT_MAX_CONTACTS) {
		hc.seen = 1;
		memcpy(&mt->sc_contacts[hc.contactid], &hc,
		    sizeof(struct hidmt_contact));
		seencontacts++;
	}

	s = spltty();
	for (i = 0; i < HIDMT_MAX_CONTACTS; i++) {
		if (!mt->sc_contacts[i].seen)
			continue;

		mt->sc_contacts[i].seen = 0;

		DPRINTF(("%s: %s: contact %d of %d: id %d, x %d, y %d, "
		    "touch %d, confidence %d, width %d, height %d "
		    "(button %d)\n",
		    mt->sc_device->dv_xname, __func__,
		    i + 1, contactcount,
		    mt->sc_contacts[i].contactid,
		    mt->sc_contacts[i].x,
		    mt->sc_contacts[i].y,
		    mt->sc_contacts[i].tip,
		    mt->sc_contacts[i].confidence,
		    mt->sc_contacts[i].width,
		    mt->sc_contacts[i].height,
		    mt->sc_button));

		if (mt->sc_contacts[i].tip && !mt->sc_contacts[i].confidence)
			continue;

		if (mt->sc_wsmode == WSMOUSE_NATIVE) {
			int width = 0;
			if (mt->sc_contacts[i].tip) {
				width = mt->sc_contacts[i].width;
				if (width < 50)
					width = 50;
			}

			wsmouse_input(mt->sc_wsmousedev, mt->sc_button,
			    (mt->last_x = mt->sc_contacts[i].x),
			    (mt->last_y = mt->sc_contacts[i].y),
			    width, tips,
			    WSMOUSE_INPUT_ABSOLUTE_X |
			    WSMOUSE_INPUT_ABSOLUTE_Y |
			    WSMOUSE_INPUT_ABSOLUTE_Z |
			    WSMOUSE_INPUT_ABSOLUTE_W);
		} else {
			wsmouse_input(mt->sc_wsmousedev, mt->sc_button,
			    (mt->last_x - mt->sc_contacts[i].x),
			    (mt->last_y - mt->sc_contacts[i].y),
			    0, 0,
			    WSMOUSE_INPUT_DELTA);
			mt->last_x = mt->sc_contacts[i].x;
			mt->last_y = mt->sc_contacts[i].y;
		}

		/*
		 * XXX: wscons can only handle one finger of data
		 */
		break;
	}

	splx(s);
}

int
hidmt_enable(struct hidmt *mt)
{
	if (mt->sc_enabled)
		return EBUSY;

	mt->sc_enabled = 1;

	return 0;
}

int
hidmt_ioctl(struct hidmt *mt, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	struct wsmouse_calibcoords *wsmc = (struct wsmouse_calibcoords *)data;
	int wsmode;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		/*
		 * So we can specify our own finger/w values to the
		 * xf86-input-synaptics driver like pms(4)
		 */
		*(u_int *)data = WSMOUSE_TYPE_ELANTECH;
		break;

	case WSMOUSEIO_GCALIBCOORDS:
		wsmc->minx = mt->sc_minx;
		wsmc->maxx = mt->sc_maxx;
		wsmc->miny = mt->sc_miny;
		wsmc->maxy = mt->sc_maxy;
		wsmc->swapxy = 0;
		wsmc->resx = 0;
		wsmc->resy = 0;
		break;

	case WSMOUSEIO_SETMODE:
		wsmode = *(u_int *)data;
		if (wsmode != WSMOUSE_COMPAT && wsmode != WSMOUSE_NATIVE) {
			printf("%s: invalid mode %d\n",
			    mt->sc_device->dv_xname, wsmode);
			return (EINVAL);
		}

		DPRINTF(("%s: changing mode to %s\n", mt->sc_device->dv_xname,
		    (wsmode == WSMOUSE_COMPAT ? "compat" : "native")));

		mt->sc_wsmode = wsmode;
		break;

	default:
		return -1;
	}

	return 0;
}

void
hidmt_disable(struct hidmt *mt)
{
	mt->sc_enabled = 0;
}
