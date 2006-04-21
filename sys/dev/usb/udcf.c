/*	$OpenBSD: udcf.c,v 1.4 2006/04/21 16:13:14 mbalmer Exp $ */

/*
 * Copyright (c) 2006 Marc Balmer <mbalmer@openbsd.org>
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
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/sensors.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#ifdef UDCF_DEBUG
#define DPRINTFN(n, x)	do { if (udcfdebug > (n)) printf x; } while (0)
int udcfdebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define SECSPERMIN	60
#define MINSPERHOUR	60
#define HOURSPERDAY	24
#define DAYSPERWEEK	7
#define DAYSPERNYEAR	365
#define DAYSPERLYEAR	366
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY	((long) SECSPERHOUR * HOURSPERDAY)
#define MONSPERYEAR	12
#define BASE_YEAR	2006
#define JAN1_2006	1136073600L	/* 2006/01/01 00:00:00 UTC */
#define LEAPS_2006	486		/* num of leap years till 2006 */

#define isleap(y)	(((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
#define leaps(y)	((y) / 4 - (y) / 100 + (y) / 400 - LEAPS_2006)

static const int	mon_lengths[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

#define UDCF_READ_REQ	0xc0
#define UDCF_READ_IDX	0x1f

#define UDCF_CTRL_REQ	0x40
#define UDCF_CTRL_IDX	0x33
#define UDCF_CTRL_VAL	0x98

struct udcf_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* USB device */
	usbd_interface_handle	sc_iface;	/* data interface */
	int			sc_refcnt;
	u_char			sc_dying;	/* disconnecting */

	struct timeout		sc_to;
	struct usb_task		sc_task;

	struct timeout		sc_bv_to;	/* bit-value detect */
	struct timeout		sc_db_to;	/* debounce */
	struct timeout		sc_mg_to;	/* minute-gap detect */
	struct timeout		sc_sl_to;	/* signal-loss detect */
	struct usb_task		sc_bv_task;
	struct usb_task		sc_mg_task;
	struct usb_task		sc_sl_task;

	usb_device_request_t	sc_req;

	int			sc_sync;	/* 1 during sync to DCF77 */
	u_int64_t		sc_mask;	/* 64 bit mask */
	u_int64_t		sc_tbits;	/* Time bits */
	int			sc_minute;
	int			sc_level;
	time_t			sc_last_mg;

	long			sc_next;	/* the time to become valid next */

	struct sensor		sc_sensor;
};

static int	t1, t2, t3, t4, t5, t6, t7;	/* timeouts in hz */

void	udcf_intr(void *);
void	udcf_probe(void *);

void	udcf_bv_intr(void *);
void	udcf_mg_intr(void *);
void	udcf_sl_intr(void *);
void	udcf_bv_probe(void *);
void	udcf_mg_probe(void *);
void	udcf_sl_probe(void *);

USB_DECLARE_DRIVER(udcf);

USB_MATCH(udcf)
{
	USB_MATCH_START(udcf, uaa);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	return uaa->vendor == USB_VENDOR_GUDE &&
	    uaa->product == USB_PRODUCT_GUDE_DCF ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

USB_ATTACH(udcf)
{
	USB_ATTACH_START(udcf, sc, uaa);
	usbd_device_handle		 dev = uaa->device;
	usbd_interface_handle		 iface;
	struct timeval			 t;
	char				*devinfop;
	usb_interface_descriptor_t	*id;
#ifdef UDCF_DEBUG
	char 				*devname = USBDEVNAME(sc->sc_dev);
#endif
	usbd_status			 err;
	usb_device_request_t		 req;
	uWord				 result;
	int				 actlen;

	if ((err = usbd_set_config_index(dev, 0, 1))) {
		DPRINTF(("\n%s: failed to set configuration, err=%s\n",
		    devname, usbd_errstr(err)));
		goto fishy;
	}

	if ((err = usbd_device2interface_handle(dev, 0, &iface))) {
		DPRINTF(("\n%s: failed to get interface, err=%s\n",
		    devname, usbd_errstr(err)));
		goto fishy;
	}

	devinfop = usbd_devinfo_alloc(dev, 0);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	id = usbd_get_interface_descriptor(iface);

	sc->sc_udev = dev;
	sc->sc_iface = iface;

	sc->sc_level = 0;
	sc->sc_minute = 0;
	sc->sc_last_mg = 0L;

	sc->sc_sync = 1;

	sc->sc_next = 0L;
	strlcpy(sc->sc_sensor.device, USBDEVNAME(sc->sc_dev),
	    sizeof(sc->sc_sensor.device));
	
	strlcpy(sc->sc_sensor.desc, "DCF77", sizeof(sc->sc_sensor.desc));
	sc->sc_sensor.type = SENSOR_TIMEDELTA;
	sc->sc_sensor.status = SENSOR_S_UNKNOWN;

	/* Prepare the USB request to probe the value */

	sc->sc_req.bmRequestType = UDCF_READ_REQ;
	sc->sc_req.bRequest = 1;
	USETW(sc->sc_req.wValue, 0);
	USETW(sc->sc_req.wIndex, UDCF_READ_IDX);
	USETW(sc->sc_req.wLength, 1);

	req.bmRequestType = UDCF_CTRL_REQ;
	req.bRequest = 0;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	if ((err = usbd_do_request_flags(sc->sc_udev, &req, &result,
	    USBD_SHORT_XFER_OK, &actlen, USBD_DEFAULT_TIMEOUT))) {
		DPRINTF(("failed to turn on power for receiver\n"));
		goto fishy;
	}

	req.bmRequestType = UDCF_CTRL_REQ;
	req.bRequest = 0;
	USETW(req.wValue, UDCF_CTRL_VAL);
	USETW(req.wIndex, UDCF_CTRL_IDX);
	USETW(req.wLength, 0);
	if ((err = usbd_do_request_flags(sc->sc_udev, &req, &result,
	    USBD_SHORT_XFER_OK, &actlen, USBD_DEFAULT_TIMEOUT))) {
		DPRINTF(("failed to turn on receiver\n"));
		goto fishy;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	usb_init_task(&sc->sc_task, udcf_probe, sc);
	usb_init_task(&sc->sc_bv_task, udcf_bv_probe, sc);
	usb_init_task(&sc->sc_mg_task, udcf_mg_probe, sc);
	usb_init_task(&sc->sc_sl_task, udcf_sl_probe, sc);

	timeout_set(&sc->sc_to, udcf_intr, sc);
	timeout_set(&sc->sc_bv_to, udcf_bv_intr, sc);
	timeout_set(&sc->sc_mg_to, udcf_mg_intr, sc);
	timeout_set(&sc->sc_sl_to, udcf_sl_intr, sc);

	/* convert timevals to hz */

	t.tv_sec = 0L;
	t.tv_usec = 150000L;
	t1 = tvtohz(&t);

	t.tv_usec = 450000L;
	t4 = tvtohz(&t);

	t.tv_usec = 900000L;
	t7 = tvtohz(&t);

	t.tv_sec = 1L;
	t.tv_usec = 500000L;
	t2 = tvtohz(&t);

	t.tv_sec = 3L;
	t.tv_usec = 0L;
	t3 = tvtohz(&t);
	
	t.tv_sec = 5L;
	t5 = tvtohz(&t);

	t.tv_sec = 8L;
	t6 = tvtohz(&t);

	/* Give the receiver some slack to stabilize */
	timeout_add(&sc->sc_to, t3);

	/* Detect signal loss in 5 sec */
	timeout_add(&sc->sc_sl_to, t5);

	/* Register the clock with kernel */

	sensor_add(&sc->sc_sensor);
	USB_ATTACH_SUCCESS_RETURN;

fishy:
	DPRINTF(("udcf_attach failed\n"));
	sc->sc_dying = 1;
	USB_ATTACH_ERROR_RETURN;
}

USB_DETACH(udcf)
{
	struct udcf_softc	*sc = (struct udcf_softc *)self;
	int			 s;

	sc->sc_dying = 1;

	timeout_del(&sc->sc_to);
	timeout_del(&sc->sc_bv_to);
	timeout_del(&sc->sc_mg_to);
	timeout_del(&sc->sc_sl_to);

	/* Unregister the clock with the kernel */

	sensor_del(&sc->sc_sensor);

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	usb_rem_task(sc->sc_udev, &sc->sc_bv_task);
	usb_rem_task(sc->sc_udev, &sc->sc_mg_task);
	usb_rem_task(sc->sc_udev, &sc->sc_sl_task);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));
	return (0);
}

/* udcf_intr runs in an interrupt context */

void
udcf_intr(void *xsc)
{
	struct udcf_softc *sc = xsc;
	usb_add_task(sc->sc_udev, &sc->sc_task);
}

/* bit value detection */

void
udcf_bv_intr(void *xsc)
{
	struct udcf_softc *sc = xsc;
	usb_add_task(sc->sc_udev, &sc->sc_bv_task);
}

/* minute gap detection */

void
udcf_mg_intr(void *xsc)
{
	struct udcf_softc *sc = xsc;
	usb_add_task(sc->sc_udev, &sc->sc_mg_task);
}

/* signal loss detection */

void
udcf_sl_intr(void *xsc)
{
	struct udcf_softc *sc = xsc;
	usb_add_task(sc->sc_udev, &sc->sc_sl_task);
}

/*
 * udcf_probe runs in a process context.  If Bit 0 is set, the transmitter
 * emits at full power.  During the low-power emission we decode a zero bit.
 */

void
udcf_probe(void *xsc)
{
	struct udcf_softc	*sc = xsc;
	struct timeval		 now;
	unsigned char		 data;
	int			 actlen;

	if (sc->sc_dying)
		return;

	if (usbd_do_request_flags(sc->sc_udev, &sc->sc_req, &data,
	    USBD_SHORT_XFER_OK, &actlen, USBD_DEFAULT_TIMEOUT))
		/* This happens if we pull the receiver */
		return;

	if (data & 0x01) {
		sc->sc_level = 1;
		timeout_add(&sc->sc_to, 1);
	} else if (sc->sc_level == 1)	{ /* Begin of a second */
		sc->sc_level = 0;
		if (sc->sc_minute == 1) {
			if (sc->sc_sync) {
				DPRINTF(("synchronized, collecting bits\n"));
				sc->sc_sync = 0;
			} else {
				/* provide the time delta */

				microtime(&now);
				sc->sc_sensor.value = (now.tv_sec - sc->sc_next) * 1000 +
				    now.tv_usec / 1000;
				sc->sc_sensor.status = SENSOR_S_OK;
			}
			sc->sc_tbits = 0LL;
			sc->sc_mask = 1LL;
			sc->sc_minute = 0;
		}

		timeout_add(&sc->sc_to, t7);	/* Begin resync in 900 ms */

		/* No bit detection during sync */
		if (!sc->sc_sync) {
			timeout_del(&sc->sc_bv_to);
			timeout_add(&sc->sc_bv_to, t1);	/* Detect bit in 150 ms */
		}
		timeout_del(&sc->sc_mg_to);
		timeout_add(&sc->sc_mg_to, t2);	/* detect minute gap in 1500 ms */
		timeout_del(&sc->sc_sl_to);
		timeout_add(&sc->sc_sl_to, t3);	/* Detect signal loss in 3 sec */
	}
}

/* detect the bit value */

void
udcf_bv_probe(void *xsc)
{
	struct udcf_softc	*sc = xsc;
	int			 actlen;
	unsigned char		 data;

	if (sc->sc_dying)
		return;

	if (usbd_do_request_flags(sc->sc_udev, &sc->sc_req, &data,
	    USBD_SHORT_XFER_OK, &actlen, USBD_DEFAULT_TIMEOUT)) {
		/* This happens if we pull the receiver */
		DPRINTF(("bit detection failed\n"));
		return;
	}

	DPRINTF((data & 0x01 ? "0" : "1"));
	if (!(data & 0x01))
		sc->sc_tbits |= sc->sc_mask;
	sc->sc_mask <<= 1;
}

/* detect the minute gap */

void
udcf_mg_probe(void *xsc)
{
	struct udcf_softc	*sc = xsc;

	int			 wday;
	int			 minute_bits, hour_bits, day_bits;
	int			 month_bits, year_bits;
	int			 p1, p2, p3;
	int			 p1_bit, p2_bit, p3_bit;
	int			 r_bit, a1_bit, a2_bit, z1_bit, z2_bit;
	int			 s_bit, m_bit;
	int			 year, mon, mday, hour, min;
	int			 nyears, lyears, days, leap;
	int			 n;

	u_int32_t		 parity = 0x6996;

	if (sc->sc_sync) {
		timeout_del(&sc->sc_to);
		timeout_add(&sc->sc_to, t4);	/* re-sync in 450 ms */
		sc->sc_minute = 1;
		sc->sc_last_mg = time_second;
	} else {
		if (time_second - sc->sc_last_mg < 57) {
			DPRINTF(("unexpected gap, resync\n"));
			sc->sc_sensor.status = SENSOR_S_UNKNOWN;
			sc->sc_sync = 1;
			timeout_del(&sc->sc_to);
			timeout_add(&sc->sc_to, t5);
			timeout_del(&sc->sc_sl_to);
			timeout_add(&sc->sc_sl_to, t6);
			sc->sc_last_mg = 0;
		} else {
			/* Extract bits w/o parity */

			m_bit = sc->sc_tbits & 1;
			r_bit = sc->sc_tbits >> 15 & 1;
			a1_bit = sc->sc_tbits >> 16 & 1;
			z1_bit = sc->sc_tbits >> 17 & 1;
			z2_bit = sc->sc_tbits >> 18 & 1;
			a2_bit = sc->sc_tbits >> 19 & 1;
			s_bit = sc->sc_tbits >> 20 & 1;
			p1_bit = sc->sc_tbits >> 28 & 1;
			p2_bit = sc->sc_tbits >> 35 & 1;
			p3_bit = sc->sc_tbits >> 58 & 1;

			minute_bits = sc->sc_tbits >> 21 & 0x7f;	
			hour_bits = sc->sc_tbits >> 29 & 0x3f;
			day_bits = sc->sc_tbits >> 36 & 0x3f;
			wday = (sc->sc_tbits >> 42) & 0x07;
			month_bits = sc->sc_tbits >> 45 & 0x1f;
			year_bits = sc->sc_tbits >> 50 & 0xff;

			/* Validate time information */

			p1 = (parity >> (minute_bits & 0x0f) & 1) ^
			    (parity >> (minute_bits >> 4) & 1);

			p2 = (parity >> (hour_bits & 0x0f) & 1) ^
			    (parity >> (hour_bits >> 4) & 1);

			p3 = (parity >> (day_bits & 0x0f) & 1) ^
			    (parity >> (day_bits >> 4) & 1) ^
			    ((parity >> wday) & 1) ^
			    (parity >> (month_bits & 0x0f) & 1) ^
			    (parity >> (month_bits >> 4) & 1) ^
			    (parity >> (year_bits & 0x0f) & 1) ^
			    (parity >> (year_bits >> 4) & 1);

			if (m_bit == 0 && s_bit == 1 &&
			    p1 == p1_bit && p2 == p2_bit &&
			    p3 == p3_bit &&
			    (z1_bit ^ z2_bit)) {

				/* Decode valid time */

				min = (minute_bits & 0x0f) +
				    (minute_bits >> 4) * 10;
				hour = (hour_bits & 0x0f) +
				    (hour_bits >> 4) * 10;
				mday = (day_bits & 0x0f) +
				    (day_bits >> 4) * 10;
				mon = (month_bits & 0x0f) +
				    (month_bits >> 4) * 10 - 1;
				year = 2000 + (year_bits & 0x0f) +
				    (year_bits >> 4) * 10;

				/* convert to coordinated universal time */
				hour -= z1_bit ? 2 : 1;
				if (hour < 0)
					hour += 24;

				if (year > BASE_YEAR)
					lyears = leaps(year - 1);
				else
					lyears = 0;
				nyears = year - BASE_YEAR - lyears;
				leap = isleap(year);

				days = nyears * DAYSPERNYEAR + lyears * DAYSPERLYEAR;
				for (n = 0; n < mon; n++)
					days += mon_lengths[leap][n];
				days += mday - 1;

				sc->sc_next = JAN1_2006 + days * SECSPERDAY +
				    hour * SECSPERHOUR + min * SECSPERMIN;

				DPRINTF(("\n%02d.%02d.%04d %02d:%02d:00 UTC",
				    mday, mon + 1, year, hour,
				    min));
				DPRINTF((z1_bit ? ", dst" : ""));
				DPRINTF((r_bit ? ", reserve antenna" : ""));
				DPRINTF((a1_bit ? ", dst chg announced" : ""));
				DPRINTF((a2_bit ? ", leap sec announced" : ""));
				DPRINTF(("\n"));
			} else {
				DPRINTF(("parity error, resync\n"));
				sc->sc_sync = 1;
			}
			timeout_del(&sc->sc_to);
			timeout_add(&sc->sc_to, t4);	/* re-sync in 450 ms */
			sc->sc_minute = 1;
			sc->sc_last_mg = time_second;
		}
	}
}

/* detect signal loss */

void
udcf_sl_probe(void *xsc)
{
	struct udcf_softc *sc = xsc;

	if (sc->sc_dying)
		return;

	DPRINTF(("signal loss, resync\n"));
	sc->sc_sensor.status = SENSOR_S_UNKNOWN;
	sc->sc_sync = 1;
	timeout_del(&sc->sc_to);
	timeout_add(&sc->sc_to, t5);
	timeout_del(&sc->sc_sl_to);
	timeout_add(&sc->sc_sl_to, t6);
}

int
udcf_activate(device_ptr_t self, enum devact act)
{
	struct udcf_softc *sc = (struct udcf_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}
