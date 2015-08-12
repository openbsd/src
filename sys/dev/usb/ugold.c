/*	$OpenBSD: ugold.c,v 1.10 2015/08/12 07:21:15 yuo Exp $   */

/*
 * Copyright (c) 2013 Takayoshi SASANO <uaa@openbsd.org>
 * Copyright (c) 2013 Martin Pieuchot <mpi@openbsd.org>
 * Copyright (c) 2015 Joerg Jung <jung@openbsd.org>
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

/*
 * Driver for Microdia's HID base TEMPer and TEMPerHUM temperature and
 * humidity sensors
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/sensors.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>

#define UGOLD_INNER		0
#define UGOLD_OUTER		1
#define UGOLD_HUM		1
#define UGOLD_MAX_SENSORS	2

#define UGOLD_CMD_DATA		0x80
#define UGOLD_CMD_INIT		0x82

#define UGOLD_TYPE_SI7005	1
#define UGOLD_TYPE_SI7006	2

/*
 * This driver uses three known commands for the TEMPer and TEMPerHUM
 * devices.
 *
 * The first byte of the answer corresponds to the command and the
 * second one seems to be the size (in bytes) of the answer.
 *
 * The device always sends 8 bytes and if the length of the answer
 * is less than that, it just leaves the last bytes untouched.  That
 * is why most of the time the last n bytes of the answers are the
 * same.
 *
 * The type command below seems to generate two answers with a
 * string corresponding to the device, for example:
 *	'TEMPer1F' and '1.1Per1F' (here Per1F is repeated).
 */
static uint8_t cmd_data[8] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };
static uint8_t cmd_init[8] = { 0x01, 0x82, 0x77, 0x01, 0x00, 0x00, 0x00, 0x00 };
static uint8_t cmd_type[8] = { 0x01, 0x86, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };

struct ugold_softc {
	struct uhidev		 sc_hdev;
	struct usbd_device	*sc_udev;

	int			 sc_num_sensors;
	int			 sc_type;

	struct ksensor		 sc_sensor[UGOLD_MAX_SENSORS];
	struct ksensordev	 sc_sensordev;
	struct sensor_task	*sc_sensortask;
};

const struct usb_devno ugold_devs[] = {
	{ USB_VENDOR_MICRODIA, USB_PRODUCT_MICRODIA_TEMPER },
	{ USB_VENDOR_MICRODIA, USB_PRODUCT_MICRODIA_TEMPERHUM },
};

int 	ugold_match(struct device *, void *, void *);
void	ugold_attach(struct device *, struct device *, void *);
int 	ugold_detach(struct device *, int);

void	ugold_ds75_intr(struct uhidev *, void *, u_int);
void	ugold_si700x_intr(struct uhidev *, void *, u_int);
void	ugold_refresh(void *);

int	ugold_issue_cmd(struct ugold_softc *, uint8_t *, int);

struct cfdriver ugold_cd = {
	NULL, "ugold", DV_DULL
};

const struct cfattach ugold_ca = {
	sizeof(struct ugold_softc), ugold_match, ugold_attach, ugold_detach,
};

int
ugold_match(struct device *parent, void *match, void *aux)
{
	struct uhidev_attach_arg *uha = aux;
	int size;
	void *desc;

	if (uha->reportid == UHIDEV_CLAIM_ALLREPORTID)
		return (UMATCH_NONE);

	if (usb_lookup(ugold_devs, uha->uaa->vendor, uha->uaa->product) == NULL)
		return (UMATCH_NONE);

	/*
	 * XXX Only match the sensor interface.
	 *
	 * Does it makes sense to attach various uhidev(4) to these
	 * non-standard HID devices?
	 */
	uhidev_get_report_desc(uha->parent, &desc, &size);
	if (hid_is_collection(desc, size, uha->reportid,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_KEYBOARD)))
		return (UMATCH_NONE);

	return (UMATCH_VENDOR_PRODUCT);

}

void
ugold_attach(struct device *parent, struct device *self, void *aux)
{
	struct ugold_softc *sc = (struct ugold_softc *)self;
	struct uhidev_attach_arg *uha = aux;
	int size, repid;
	void *desc;

	sc->sc_udev = uha->parent->sc_udev;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;
	switch (uha->uaa->product) {
	case USB_PRODUCT_MICRODIA_TEMPER:
		sc->sc_hdev.sc_intr = ugold_ds75_intr;
		break;
	case USB_PRODUCT_MICRODIA_TEMPERHUM:
		sc->sc_hdev.sc_intr = ugold_si700x_intr;
		break;
	default:
		printf(", unknown product\n");
		return;
	}

	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_hdev.sc_isize = hid_report_size(desc, size, hid_input, repid);
	sc->sc_hdev.sc_osize = hid_report_size(desc, size, hid_output, repid);
	sc->sc_hdev.sc_fsize = hid_report_size(desc, size, hid_feature, repid);

	if (uhidev_open(&sc->sc_hdev)) {
		printf(", unable to open interrupt pipe\n");
		return;
	}

	strlcpy(sc->sc_sensordev.xname, sc->sc_hdev.sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	switch (uha->uaa->product) {
	case USB_PRODUCT_MICRODIA_TEMPER:
		/* 2 temperature sensors */
		sc->sc_sensor[UGOLD_INNER].type = SENSOR_TEMP;
		strlcpy(sc->sc_sensor[UGOLD_INNER].desc, "inner",
		    sizeof(sc->sc_sensor[UGOLD_INNER].desc));
		sc->sc_sensor[UGOLD_OUTER].type = SENSOR_TEMP;
		strlcpy(sc->sc_sensor[UGOLD_OUTER].desc, "outer",
		    sizeof(sc->sc_sensor[UGOLD_OUTER].desc));
		break;
	case USB_PRODUCT_MICRODIA_TEMPERHUM:
		/* 1 temperature and 1 humidity sensor */
		sc->sc_sensor[UGOLD_INNER].type = SENSOR_TEMP;
		strlcpy(sc->sc_sensor[UGOLD_INNER].desc, "inner",
		    sizeof(sc->sc_sensor[UGOLD_INNER].desc));
		sc->sc_sensor[UGOLD_HUM].type = SENSOR_HUMIDITY;
		strlcpy(sc->sc_sensor[UGOLD_HUM].desc, "RH",
		    sizeof(sc->sc_sensor[UGOLD_HUM].desc));
		break;
	default:
		printf(", unknown product\n");
		return;
	}

	/* 0.1Hz */
	sc->sc_sensortask = sensor_task_register(sc, ugold_refresh, 6);
	if (sc->sc_sensortask == NULL) {
		printf(", unable to register update task\n");
		return;
	}
	printf("\n");

	sensordev_install(&sc->sc_sensordev);
}

int
ugold_detach(struct device *self, int flags)
{
	struct ugold_softc *sc = (struct ugold_softc *)self;
	int i;

	if (sc->sc_sensortask != NULL) {
		sensor_task_unregister(sc->sc_sensortask);
		sensordev_deinstall(&sc->sc_sensordev);
	}

	for (i = 0; i < sc->sc_num_sensors; i++)
		sensor_detach(&sc->sc_sensordev, &sc->sc_sensor[i]);

	if (sc->sc_hdev.sc_state & UHIDEV_OPEN)
		uhidev_close(&sc->sc_hdev);

	return (0);
}

static int
ugold_ds75_temp(uint8_t msb, uint8_t lsb)
{
	/* DS75 12bit precision mode: 0.0625 degrees Celsius ticks */
	return (((msb * 100) + ((lsb >> 4) * 25 / 4)) * 10000) + 273150000;
}

static void
ugold_ds75_type(struct ugold_softc *sc, uint8_t *buf, u_int len)
{
	if (memcmp(buf, "TEMPer1F", len) == 0 ||
	    memcmp(buf, "TEMPer2F", len) == 0 ||
	    memcmp(buf, "TEMPerF1", len) == 0)
		return; /* skip first half of the answer */

	printf("%s: %d sensor%s type ds75/12bit (temperature)\n",
	    sc->sc_hdev.sc_dev.dv_xname, sc->sc_num_sensors,
	    (sc->sc_num_sensors == 1) ? "" : "s");

	sc->sc_type = -1; /* ignore type */
}

void
ugold_ds75_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	struct ugold_softc *sc = (struct ugold_softc *)addr;
	uint8_t *buf = ibuf;
	int i, temp;

	switch (buf[0]) {
	case UGOLD_CMD_INIT:
		if (sc->sc_num_sensors)
			break;

		sc->sc_num_sensors = min(buf[1], UGOLD_MAX_SENSORS) /* XXX */;

		for (i = 0; i < sc->sc_num_sensors; i++) {
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[i]);
		}

		break;
	case UGOLD_CMD_DATA:
		switch (buf[1]) {
		case 4:
			temp = ugold_ds75_temp(buf[4], buf[5]);
			sc->sc_sensor[UGOLD_OUTER].value = temp;
			sc->sc_sensor[UGOLD_OUTER].flags &= ~SENSOR_FINVALID;
			/* FALLTHROUGH */
		case 2:
			temp = ugold_ds75_temp(buf[2], buf[3]);
			sc->sc_sensor[UGOLD_INNER].value = temp;
			sc->sc_sensor[UGOLD_INNER].flags &= ~SENSOR_FINVALID;
			break;
		default:
			printf("%s: invalid data length (%d bytes)\n",
				sc->sc_hdev.sc_dev.dv_xname, buf[1]);
		}
		break;
	default:
		if (!sc->sc_type) { /* type command returns arbitrary string */
			ugold_ds75_type(sc, buf, len);
			break;
		}
		printf("%s: unknown command 0x%02x\n",
		    sc->sc_hdev.sc_dev.dv_xname, buf[0]);
	}
}

static int
ugold_si700x_temp(int type, uint8_t msb, uint8_t lsb)
{
	int temp = msb * 256 + lsb;

	switch (type) { /* convert to mdegC */
	case UGOLD_TYPE_SI7005: /* 14bit 32 codes per degC 0x0000 = -50 degC */
		temp = (((temp & 0x3fff) * 1000) / 32) - 50000;
		break;
	case UGOLD_TYPE_SI7006: /* 14bit and status bit */
		temp = (((temp & ~3) * 21965) / 8192) - 46850;
		break;
	default:
		temp = 0;
	}
	return temp;
}

static int
ugold_si700x_rhum(int type, uint8_t msb, uint8_t lsb, int temp)
{
	int rhum = msb * 256 + lsb;

	switch (type) { /* convert to m%RH */
	case UGOLD_TYPE_SI7005: /* 12bit 16 codes per %RH 0x0000 = -24 %RH */
		rhum = (((rhum & 0x0fff) * 1000) / 16) - 24000;
#if 0		/* todo: linearization and temperature compensation */
		rhum -= -0.00393 * rhum * rhum + 0.4008 * rhum - 4.7844;
		rhum += (temp - 30) * (0.00237 * rhum + 0.1973);
#endif
		break;
	case UGOLD_TYPE_SI7006: /* 14bit and status bit */
		rhum = (((rhum & ~3) * 15625) / 8192) - 6000;
		break;
	default:
		rhum = 0;
	}

	/* limit the humidity to valid values */
	if (rhum < 0)
		rhum = 0;
	else if (rhum > 100000)
		rhum = 100000;
	return rhum;
}

static void
ugold_si700x_type(struct ugold_softc *sc, uint8_t *buf, u_int len)
{
	if (memcmp(buf, "TEMPerHu", len) == 0)
		return; /* skip equal first half of the answer */

	printf("%s: %d sensor%s type ", sc->sc_hdev.sc_dev.dv_xname,
	    sc->sc_num_sensors, (sc->sc_num_sensors == 1) ? "" : "s");

	if (memcmp(buf, "mM12V1.0", len) == 0) {
		sc->sc_type = UGOLD_TYPE_SI7005;
		printf("si7005 (temperature and humidity)\n");
	} else if (memcmp(buf, "mM12V1.2", len) == 0) {
		sc->sc_type = UGOLD_TYPE_SI7006;
		printf("si7006 (temperature and humidity)\n");
	} else {
		sc->sc_type = -1;
		printf("unknown\n");
	}
}

void
ugold_si700x_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	struct ugold_softc *sc = (struct ugold_softc *)addr;
	uint8_t *buf = ibuf;
	int i, temp, rhum;

	switch (buf[0]) {
	case UGOLD_CMD_INIT:
		if (sc->sc_num_sensors)
			break;

		sc->sc_num_sensors = min(buf[1], UGOLD_MAX_SENSORS) /* XXX */;

		for (i = 0; i < sc->sc_num_sensors; i++) {
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[i]);
		}
		break;
	case UGOLD_CMD_DATA:
		if (buf[1] != 4)
			printf("%s: invalid data length (%d bytes)\n",
			    sc->sc_hdev.sc_dev.dv_xname, buf[1]);
		temp = ugold_si700x_temp(sc->sc_type, buf[2], buf[3]);
		sc->sc_sensor[UGOLD_INNER].value = (temp * 1000) + 273150000;
		sc->sc_sensor[UGOLD_INNER].flags &= ~SENSOR_FINVALID;
		rhum = ugold_si700x_rhum(sc->sc_type, buf[4], buf[5], temp);
		sc->sc_sensor[UGOLD_HUM].value = rhum;
		sc->sc_sensor[UGOLD_HUM].flags &= ~SENSOR_FINVALID;
		break;
	default:
		if (!sc->sc_type) { /* type command returns arbitrary string */
			ugold_si700x_type(sc, buf, len);
			break;
		}
		printf("%s: unknown command 0x%02x\n",
		    sc->sc_hdev.sc_dev.dv_xname, buf[0]);
	}
}

void
ugold_refresh(void *arg)
{
	struct ugold_softc *sc = arg;
	int i;

	if (!sc->sc_num_sensors) {
		ugold_issue_cmd(sc, cmd_init, sizeof(cmd_init));
		return;
	}
	if (!sc->sc_type) {
		ugold_issue_cmd(sc, cmd_type, sizeof(cmd_type));
		return;
	}

	if (ugold_issue_cmd(sc, cmd_data, sizeof(cmd_data))) {
		for (i = 0; i < sc->sc_num_sensors; i++)
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
	}
}

int
ugold_issue_cmd(struct ugold_softc *sc, uint8_t *cmd, int len)
{
	int actlen;

	actlen = uhidev_set_report_async(sc->sc_hdev.sc_parent,
	    UHID_OUTPUT_REPORT, sc->sc_hdev.sc_report_id, cmd, len);
	return (actlen != len);
}
