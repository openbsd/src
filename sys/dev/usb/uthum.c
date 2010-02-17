/*	$OpenBSD: uthum.c,v 1.7 2010/02/17 14:06:10 yuo Exp $   */

/*
 * Copyright (c) 2009 Yojiro UO <yuo@nui.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Driver for TEMPerHUM HID */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/sensors.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>

#ifdef USB_DEBUG
#define UTHUM_DEBUG
#endif

#ifdef UTHUM_DEBUG
int	uthumdebug = 0;
#define DPRINTFN(n, x)	do { if (uthumdebug > (n)) printf x; } while (0)
#else
#define DPRINTFN(n, x)
#endif

#define DPRINTF(x) DPRINTFN(0, x)


/* TEMPerHUM */
#define CMD_DEVTYPE 0x52 /* XXX */
#define CMD_GETDATA 0x48 /* XXX */
static uint8_t cmd_start[8] =
	{ 0x0a, 0x0b, 0x0c, 0x0d, 0x00, 0x00, 0x02, 0x00 };
static uint8_t cmd_end[8] =
	{ 0x0a, 0x0b, 0x0c, 0x0d, 0x00, 0x00, 0x01, 0x00 };

/* sensors */
#define UTHUM_TEMP		0
#define UTHUM_HUMIDITY		1
#define UTHUM_MAX_SENSORS	2

#define UTHUM_TYPE_UNKNOWN	0
#define UTHUM_TYPE_SHT1x	1

struct uthum_softc {
	struct uhidev		 sc_hdev;
	usbd_device_handle	 sc_udev;
	u_char			 sc_dying;
	uint16_t		 sc_flag;
	int			 sc_sensortype;

	/* uhidev parameters */
	size_t			 sc_flen;	/* feature report length */
	size_t			 sc_ilen;	/* input report length */
	size_t			 sc_olen;	/* output report length */

	/* sensor framework */
	struct ksensor		 sc_sensor[UTHUM_MAX_SENSORS];
	struct ksensordev	 sc_sensordev;
	struct sensor_task	*sc_sensortask;

	uint8_t			 sc_num_sensors;
};


const struct usb_devno uthum_devs[] = {
	/* XXX: various TEMPer variants using same VID/PID */
	{ USB_VENDOR_TENX, USB_PRODUCT_TENX_TEMPER},
};
#define uthum_lookup(v, p) usb_lookup(uthum_devs, v, p)

int uthum_match(struct device *, void *, void *);
void uthum_attach(struct device *, struct device *, void *);
int uthum_detach(struct device *, int);
int uthum_activate(struct device *, int);

int uthum_read_data(struct uthum_softc *, uint8_t, uint8_t *, size_t, int);
int uthum_check_sensortype(struct uthum_softc *);
int uthum_sht1x_temp(unsigned int);
int uthum_sht1x_rh(unsigned int, int);

void uthum_intr(struct uhidev *, void *, u_int);
void uthum_refresh(void *);

struct cfdriver uthum_cd = {
	NULL, "uthum", DV_DULL
};

const struct cfattach uthum_ca = {
	sizeof(struct uthum_softc),
	uthum_match,
	uthum_attach,
	uthum_detach,
	uthum_activate,
};

int
uthum_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;

	if (uthum_lookup(uha->uaa->vendor, uha->uaa->product) == NULL)
		return UMATCH_NONE;

#if 0 /* attach only sensor part of HID as uthum* */
#define HUG_UNKNOWN_3	0x0003
	void *desc;
	int size;
	uhidev_get_report_desc(uha->parent, &desc, &size);
	if (!hid_is_collection(desc, size, uha->reportid,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_UNKNOWN_3)))
		return (UMATCH_NONE);
#undef HUG_UNKNOWN_3
#endif

	return (UMATCH_VENDOR_PRODUCT);
}

void
uthum_attach(struct device *parent, struct device *self, void *aux)
{
	struct uthum_softc *sc = (struct uthum_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	usbd_device_handle dev = uha->parent->sc_udev;
	int size, repid;
	void *desc;

	sc->sc_udev = dev;
	sc->sc_hdev.sc_intr = uthum_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;
	sc->sc_num_sensors = 0;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_ilen = hid_report_size(desc, size, hid_input, repid);
	sc->sc_olen = hid_report_size(desc, size, hid_output, repid);
	sc->sc_flen = hid_report_size(desc, size, hid_feature, repid);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    &sc->sc_hdev.sc_dev);
	printf("\n");

	if (sc->sc_flen < 32) {
		/* not sensor interface, just attach */
		return;
	}

	sc->sc_sensortype = uthum_check_sensortype(sc);

	/* attach sensor */
	strlcpy(sc->sc_sensordev.xname, sc->sc_hdev.sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	switch (sc->sc_sensortype) {
	case UTHUM_TYPE_SHT1x:
		strlcpy(sc->sc_sensor[UTHUM_TEMP].desc, "temp",
		    sizeof(sc->sc_sensor[UTHUM_TEMP].desc));
		sc->sc_sensor[UTHUM_TEMP].type = SENSOR_TEMP;
		sc->sc_sensor[UTHUM_TEMP].status = SENSOR_S_UNSPEC;
		sc->sc_sensor[UTHUM_TEMP].flags = SENSOR_FINVALID;

		strlcpy(sc->sc_sensor[UTHUM_HUMIDITY].desc, "humidity",
		    sizeof(sc->sc_sensor[UTHUM_HUMIDITY].desc));
		sc->sc_sensor[UTHUM_HUMIDITY].type = SENSOR_PERCENT;
		sc->sc_sensor[UTHUM_HUMIDITY].value = 0;
		sc->sc_sensor[UTHUM_HUMIDITY].status = SENSOR_S_UNSPEC;
		sc->sc_sensor[UTHUM_HUMIDITY].flags = SENSOR_FINVALID;

		sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[UTHUM_TEMP]);
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[UTHUM_HUMIDITY]);
		sc->sc_num_sensors = 2;
		DPRINTF(("%s: sensor type: SHT1x\n"));
		break;
	case UTHUM_TYPE_UNKNOWN:
		DPRINTF(("%s: sensor type: unknown, give up to attach sensors\n"));
	default:
		break;
	}

	if (sc->sc_num_sensors > 0) {
		sc->sc_sensortask = sensor_task_register(sc, uthum_refresh, 20);
		if (sc->sc_sensortask == NULL) {
			printf(", unable to register update task\n");
			return;
		}
		sensordev_install(&sc->sc_sensordev);
	}

	DPRINTF(("uthum_attach: complete\n"));
}

int
uthum_detach(struct device *self, int flags)
{
	struct uthum_softc *sc = (struct uthum_softc *)self;
	int i, rv = 0;

	sc->sc_dying = 1;

	if (sc->sc_num_sensors > 0) {
		wakeup(&sc->sc_sensortask);
		sensordev_deinstall(&sc->sc_sensordev);
		for (i = 0; i < sc->sc_num_sensors; i++)
			sensor_detach(&sc->sc_sensordev, &sc->sc_sensor[i]);
		if (sc->sc_sensortask != NULL)
			sensor_task_unregister(sc->sc_sensortask);
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    &sc->sc_hdev.sc_dev);

	return (rv);
}

int
uthum_activate(struct device *self, int act)
{
	struct uthum_softc *sc = (struct uthum_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}

void
uthum_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	/* do nothing */
}

int
uthum_read_data(struct uthum_softc *sc, uint8_t target_cmd, uint8_t *buf,
	size_t len, int delay)
{
	int i;
	uint8_t cmdbuf[32], report[256];

	/* if return buffer is null, do nothing */
	if ((buf == NULL) || len == 0)
		return 0;

	/* issue query */
	bzero(cmdbuf, sizeof(cmdbuf));
	memcpy(cmdbuf, cmd_start, sizeof(cmd_start));
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT,
	    cmdbuf, sc->sc_olen))
		return EIO;

	bzero(cmdbuf, sizeof(cmdbuf));
	cmdbuf[0] = target_cmd;
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT,
	    cmdbuf, sc->sc_olen))
		return EIO;

	bzero(cmdbuf, sizeof(cmdbuf));
	for (i = 0; i < 7; i++) {
		if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT,
		    cmdbuf, sc->sc_olen))
			return EIO;
	}
	memcpy(cmdbuf, cmd_end, sizeof(cmd_end));
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT,
	    cmdbuf, sc->sc_olen))
		return EIO;

	/* wait if required */
	if (delay > 0)
		tsleep(&sc->sc_sensortask, 0, "uthum", (delay*hz+999)/1000 + 1);

	/* get answer */
	if (uhidev_get_report(&sc->sc_hdev, UHID_FEATURE_REPORT,
	    report, sc->sc_flen))
		return EIO;
	memcpy(buf, report, len);
	return 0;
}

int
uthum_check_sensortype(struct uthum_softc *sc)
{
	uint8_t buf[8];
	static uint8_t sht1x_sig[] =
	    { 0x57, 0x5a, 0x13, 0x00, 0x14, 0x00, 0x53, 0x00 };

	if (uthum_read_data(sc, CMD_DEVTYPE, buf, sizeof(buf), 0) != 0) {
		DPRINTF(("uthum: read fail\n"));
		return UTHUM_TYPE_UNKNOWN;
	}

	/*
	 * currently we have not enough information about the return value,
	 * therefore, compare full bytes.
	 * TEMPerHUM HID (SHT1x version) will return:
	 *   { 0x57, 0x5a, 0x13, 0x00, 0x14, 0x00, 0x53, 0x00 }
	 */
	if (memcmp(buf, sht1x_sig, sizeof(sht1x_sig)))
		return UTHUM_TYPE_SHT1x;

	return UTHUM_TYPE_UNKNOWN;
}


void
uthum_refresh(void *arg)
{
	struct uthum_softc *sc = arg;
	uint8_t buf[8];
	unsigned int temp_tick, humidity_tick;
	int temp, rh;

	if (uthum_read_data(sc, CMD_GETDATA, buf, sizeof(buf), 1000) != 0) {
		DPRINTF(("uthum: data read fail\n"));
		sc->sc_sensor[UTHUM_TEMP].flags |= SENSOR_FINVALID;
		sc->sc_sensor[UTHUM_HUMIDITY].flags |= SENSOR_FINVALID;
		return;
	}

	switch (sc->sc_sensortype) {
	case UTHUM_TYPE_SHT1x:
		temp_tick = (buf[0] * 256 + buf[1]) & 0x3fff;
		humidity_tick = (buf[2] * 256 + buf[3]) & 0x0fff;

		temp = uthum_sht1x_temp(temp_tick);
		rh = uthum_sht1x_rh(humidity_tick, temp);
		break;
	default:
		/* do nothing */
		return;
	}

	sc->sc_sensor[UTHUM_TEMP].value = (temp * 10000) + 273150000;
	sc->sc_sensor[UTHUM_TEMP].flags &= ~SENSOR_FINVALID;
	sc->sc_sensor[UTHUM_HUMIDITY].value = rh;
	sc->sc_sensor[UTHUM_HUMIDITY].flags &= ~SENSOR_FINVALID;
}

/* return C-degree * 100 value */
int
uthum_sht1x_temp(unsigned int ticks)
{
	/* 
	 * VDD		constant
	 *-----------------------
	 * 5.0V		-4010
	 * 4.0V		-3980
	 * 3.5V		-3970
	 * 3.0V		-3960
	 * 2.5V		-3940
	 */
	/*
	 * as the VDD of the SHT10 on my TEMPerHUM is 3.43V +/- 0.05V,
	 * I choose -3970 as the constant of this formula.
	 */
	return (ticks - 3970);
}

/* return %RH * 1000 */
int
uthum_sht1x_rh(unsigned int ticks, int temp)
{
	int rh_l, rh;

	rh_l = (-40000 + 405 * ticks) - ((7 * ticks * ticks) / 250);
	rh = ((temp - 2500) * (1 + (ticks >> 7)) + rh_l) / 10;
	return rh;
}
