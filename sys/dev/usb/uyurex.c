/*	$OpenBSD: uyurex.c,v 1.2 2010/03/03 19:08:02 miod Exp $ */

/*
 * Copyright (c) 2010 Yojiro UO <yuo@nui.org>
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

/* Driver for Maywa-Denki & KAYAC YUREX BBU sensor */

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
#define UYUREX_DEBUG
#endif

#define	CMD_NONE	0xf0
#define CMD_EOF		0x0d
#define CMD_ACK		0x21
#define	CMD_MODE	0x41 /* XXX */
#define	CMD_VALUE	0x43
#define CMD_READ	0x52
#define CMD_WRITE	0x53
#define CMD_PADDING	0xff

#define UPDATE_TICK	5 /* sec */

#ifdef UYUREX_DEBUG
int	uyurexdebug = 0;
#define DPRINTFN(n, x)	do { if (uyurexdebug > (n)) printf x; } while (0)
#else
#define DPRINTFN(n, x)
#endif

#define DPRINTF(x) DPRINTFN(0, x)

struct uyurex_softc {
	struct uhidev		 sc_hdev;
	usbd_device_handle	 sc_udev;
	u_char			 sc_dying;
	uint16_t		 sc_flag;

	/* uhidev parameters */
	size_t			 sc_flen;	/* feature report length */
	size_t			 sc_ilen;	/* input report length */
	size_t			 sc_olen;	/* output report length */

	uint8_t			*sc_ibuf;

	/* sensor framework */
	struct ksensor		 sc_sensor_val;
	struct ksensor		 sc_sensor_delta;
	struct ksensordev	 sc_sensordev;
	struct sensor_task	*sc_sensortask;

	/* device private */
	int			 sc_notinit;
	uint8_t			 issueing_cmd;
	uint8_t			 accepted_cmd;

	uint32_t		 sc_curval;
	uint32_t		 sc_oldval;
};

const struct usb_devno uyurex_devs[] = {
	{ USB_VENDOR_MICRODIA, USB_PRODUCT_MICRODIA_YUREX},
};
#define uyurex_lookup(v, p) usb_lookup(uyurex_devs, v, p)

int uyurex_match(struct device *, void *, void *);
void uyurex_attach(struct device *, struct device *, void *);
int uyurex_detach(struct device *, int);
int uyurex_activate(struct device *, int);

void uyurex_set_mode(struct uyurex_softc *, uint8_t);
void uyurex_read_value_request(struct uyurex_softc *);
void uyurex_write_value_request(struct uyurex_softc *, uint32_t);

void uyurex_intr(struct uhidev *, void *, u_int);
void uyurex_refresh(void *);

struct cfdriver uyurex_cd = {
	NULL, "uyurex", DV_DULL
};

const struct cfattach uyurex_ca = {
	sizeof(struct uyurex_softc),
	uyurex_match,
	uyurex_attach,
	uyurex_detach,
	uyurex_activate,
};

int
uyurex_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;

	if (uyurex_lookup(uha->uaa->vendor, uha->uaa->product) == NULL)
		return UMATCH_NONE;

	return (UMATCH_VENDOR_PRODUCT);
}

void
uyurex_attach(struct device *parent, struct device *self, void *aux)
{
	struct uyurex_softc *sc = (struct uyurex_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	usbd_device_handle dev = uha->parent->sc_udev;
	int size, repid, err;
	void *desc;

	sc->sc_udev = dev;
	sc->sc_hdev.sc_intr = uyurex_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_ilen = hid_report_size(desc, size, hid_input, repid);
	sc->sc_olen = hid_report_size(desc, size, hid_output, repid);
	sc->sc_flen = hid_report_size(desc, size, hid_feature, repid);

	err = uhidev_open(&sc->sc_hdev);
	if (err) {
		printf("uyurex_open: uhidev_open %d\n", err);
		return;
	}
	sc->sc_ibuf = malloc(sc->sc_ilen, M_USBDEV, M_WAITOK);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    &sc->sc_hdev.sc_dev);
	printf("\n");


	/* attach sensor */
	strlcpy(sc->sc_sensordev.xname, sc->sc_hdev.sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));


	/* add BBU sensor */
	sc->sc_sensor_val.type = SENSOR_INTEGER;
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor_val);
	strlcpy(sc->sc_sensor_val.desc, "BBU",
		sizeof(sc->sc_sensor_val.desc));

	/* add BBU delta sensor */
	sc->sc_sensor_delta.type = SENSOR_INTEGER;
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor_delta);
	strlcpy(sc->sc_sensor_delta.desc, "mBBU/sec",
		sizeof(sc->sc_sensor_delta.desc));

	sc->sc_sensortask = sensor_task_register(sc, uyurex_refresh, UPDATE_TICK);
	if (sc->sc_sensortask == NULL) {
		printf(", unable to register update task\n");
		return;
	}
	sensordev_install(&sc->sc_sensordev);

	sc->sc_curval = sc->sc_oldval = 0;

	DPRINTF(("uyurex_attach: complete\n"));

	/* init device */ /* XXX */
	sc->sc_notinit = 1;
	uyurex_set_mode(sc, 0);
}

int
uyurex_detach(struct device *self, int flags)
{
	struct uyurex_softc *sc = (struct uyurex_softc *)self;
	int rv = 0;

	sc->sc_dying = 1;

	wakeup(&sc->sc_sensortask);
	sensordev_deinstall(&sc->sc_sensordev);
	sensor_detach(&sc->sc_sensordev, &sc->sc_sensor_val);
	sensor_detach(&sc->sc_sensordev, &sc->sc_sensor_delta);
	if (sc->sc_sensortask != NULL)
		sensor_task_unregister(sc->sc_sensortask);

	if (sc->sc_ibuf != NULL) {
		free(sc->sc_ibuf, M_USBDEV);
		sc->sc_ibuf = NULL;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    &sc->sc_hdev.sc_dev);

	return (rv);
}

int
uyurex_activate(struct device *self, int act)
{
	struct uyurex_softc *sc = (struct uyurex_softc *)self;

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
uyurex_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	struct uyurex_softc *sc = (struct uyurex_softc *)addr;
	uint8_t buf[8];
	uint32_t val;

	if (sc->sc_ibuf == NULL)
		return;

	/* process requests */
	memcpy(buf, ibuf, 8);
	DPRINTF(("intr: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n",
		buf[0], buf[1], buf[2], buf[3],
		buf[4], buf[5], buf[6], buf[7]));


	switch (buf[0]) {
	case CMD_ACK:
		if (buf[1] == sc->issueing_cmd) {
			DPRINTF(("ack recieved for cmd 0x%.2x\n", buf[1]));
			sc->accepted_cmd = buf[1];
		} else {
			DPRINTF(("cmd-ack mismatch: recved 0x%.2x, expect 0x%.2x\n",
				buf[1], sc->issueing_cmd));
			/* discard previous command */
			sc->accepted_cmd = CMD_NONE;
			sc->issueing_cmd = CMD_NONE;
		}
		break;
	case CMD_READ:
	case CMD_VALUE:
		val = (buf[2] << 24) + (buf[3] << 16) + (buf[4] << 8)  + buf[5];
		if (sc->sc_notinit) {
			sc->sc_oldval = val;
			sc->sc_notinit = 0; 
		}
		sc->sc_sensor_val.value = val;
		sc->sc_curval = val;
		DPRINTF(("recv value update message: %d\n", val));
		break;
	default:
		DPRINTF(("unknown message: 0x%.2x\n", buf[0]));
	}

	return;
}

void
uyurex_refresh(void *arg)
{
	struct uyurex_softc *sc = arg;

	if (sc->sc_notinit) {
		uyurex_read_value_request(sc);
	} else {
		/* calculate delta value */
		sc->sc_sensor_delta.value = 
			(1000 * (sc->sc_curval - sc->sc_oldval)) / UPDATE_TICK;
		sc->sc_oldval = sc->sc_curval;
	} 
}

void
uyurex_set_mode(struct uyurex_softc *sc, uint8_t val)
{
	uint8_t req[8];
	usbd_status err;

	memset(req, CMD_PADDING, sizeof(req));
	req[0] = CMD_MODE;
	req[1] = val;
	req[2] = CMD_EOF;
	err = uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT, req,
		sc->sc_olen);
	if (err) {
		printf("uhidev_set_report error:EIO\n");
		return;
	}

	/* wait ack */
	tsleep(&sc->sc_sensortask, 0, "uyurex", (1000*hz+999)/1000 + 1);
}

void
uyurex_read_value_request(struct uyurex_softc *sc)
{
	uint8_t req[8];

	memset(req, CMD_PADDING, sizeof(req));
	req[0] = CMD_READ;
	req[1] = CMD_EOF;
	sc->issueing_cmd = CMD_READ;
	sc->accepted_cmd = CMD_NONE;
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT, req,
		sc->sc_olen))
		return;
		
	/* wait till sensor data are updated, 500ms will be enough */
	tsleep(&sc->sc_sensortask, 0, "uyurex", (500*hz+999)/1000 + 1);
}

void
uyurex_write_value_request(struct uyurex_softc *sc, uint32_t val)
{
	uint32_t v;
	uint8_t req[8];

	req[0] = CMD_WRITE;
	req[1] = 0;
	req[6] = CMD_EOF;
	req[7] = CMD_PADDING;
	v = htobe32(val);
	memcpy(req + 2, &v, sizeof(uint32_t));

	sc->issueing_cmd = CMD_WRITE;
	sc->accepted_cmd = CMD_NONE;
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT, req,
		sc->sc_olen))
		return;

	/* wait till sensor data are updated, 250ms will be enough */
	tsleep(&sc->sc_sensortask, 0, "uyurex", (250*hz+999)/1000 + 1);
}
