/*	$OpenBSD: upd.c,v 1.2 2014/03/19 16:08:32 deraadt Exp $ */

/*
 * Copyright (c) 2014 Andre de Oliveira <andre@openbsd.org>
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

/* Driver for USB Power Devices sensors */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/sensors.h>

#include <dev/usb/hid.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/usbdi_util.h>

#ifdef UPD_DEBUG
#define DPRINTF(x)	do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

enum upd_sensor_id {
	UPD_SENSOR_UNKNOWN,
	UPD_SENSOR_RELCHARGE,
	UPD_SENSOR_ABSCHARGE,
	UPD_SENSOR_REMCAPACI,
	UPD_SENSOR_FULLCHARG,
	UPD_SENSOR_CHARGING,
	UPD_SENSOR_DISCHARG,
	UPD_SENSOR_BATTPRESENT,
	UPD_SENSOR_SHUTIMMINENT,
	UPD_SENSOR_ACPRESENT,
	UPD_SENSOR_TIMETOFULL,
	UPD_SENSOR_NUM
	/*
	 * TODO
	 * - atratetimetofull
	 * - atratetimetoempty
	 * - cyclecount
	 */
};

struct upd_usage_entry {
	enum upd_sensor_id	upd_sid;
	uint8_t			usage_pg;
	uint8_t			usage_id;
	enum sensor_type	senstype;
	char			*usage_name; /* sensor string */
};

static struct upd_usage_entry upd_usage_table[UPD_SENSOR_NUM] = {
	{ UPD_SENSOR_UNKNOWN,	  HUP_UNDEFINED,HUP_UNDEFINED,
	    -1,			 "unknown" },
	{ UPD_SENSOR_RELCHARGE,	  HUP_BATTERY,	HUB_REL_STATEOF_CHARGE,
	    SENSOR_PERCENT,	 "RelativeStateOfCharge" },
	{ UPD_SENSOR_ABSCHARGE,	  HUP_BATTERY,	HUB_ABS_STATEOF_CHARGE,
	    SENSOR_PERCENT,	 "AbsoluteStateOfCharge" },
	{ UPD_SENSOR_REMCAPACI,	  HUP_BATTERY,	HUB_REM_CAPACITY,
	    SENSOR_PERCENT,	 "RemainingCapacity" },
	{ UPD_SENSOR_FULLCHARG,	  HUP_BATTERY,	HUB_FULLCHARGE_CAPACITY,
	    SENSOR_PERCENT,	 "FullChargeCapacity" },
	{ UPD_SENSOR_CHARGING,	  HUP_BATTERY,	HUB_CHARGING,
	    SENSOR_INDICATOR,	 "Charging" },
	{ UPD_SENSOR_DISCHARG,	  HUP_BATTERY,	HUB_DISCHARGING,
	    SENSOR_INDICATOR,	 "Discharging" },
	{ UPD_SENSOR_BATTPRESENT, HUP_BATTERY,	HUB_BATTERY_PRESENT,
	    SENSOR_INDICATOR,	 "BatteryPresent" },
	{ UPD_SENSOR_SHUTIMMINENT,HUP_POWER,	HUP_SHUTDOWN_IMMINENT,
	    SENSOR_INDICATOR,	 "ShutdownImminent" },
	{ UPD_SENSOR_ACPRESENT,	  HUP_BATTERY,	HUB_AC_PRESENT,
	    SENSOR_INDICATOR,	 "ACPresent" },
	{ UPD_SENSOR_TIMETOFULL,  HUP_BATTERY,	HUB_ATRATE_TIMETOFULL,
	    SENSOR_TIMEDELTA,	 "AtRateTimeToFull" }
};

struct upd_sensor {
	int			attached;
	struct ksensor		sensor;
	struct hid_item		item;
	size_t			flen;
};

struct upd_softc {
	struct uhidev		 sc_hdev;
	int			 sc_num_sensors;

	/* sensor framework */
	struct upd_sensor	 sc_sensors[UPD_SENSOR_NUM];
	struct ksensordev	 sc_sensordev;
	struct sensor_task	*sc_sensortask;
};

static const struct usb_devno upd_devs[] = {
	{ USB_VENDOR_APC, USB_PRODUCT_APC_UPS },
	{ USB_VENDOR_APC, USB_PRODUCT_APC_UPS5G },
	{ USB_VENDOR_LIEBERT, USB_PRODUCT_LIEBERT_UPS }
};
#define upd_lookup(v, p) usb_lookup(upd_devs, v, p)

int  upd_match(struct device *, void *, void *);
void upd_attach(struct device *, struct device *, void *);
int  upd_detach(struct device *, int);

void upd_add_sensor(struct upd_softc *, const struct hid_item *, void *, int);
void upd_refresh(void *);
void upd_intr(struct uhidev *, void *, uint);

struct cfdriver upd_cd = {
	NULL, "upd", DV_DULL
};

const struct cfattach upd_ca = {
	sizeof(struct upd_softc),
	upd_match,
	upd_attach,
	upd_detach
};

int
upd_match(struct device *parent, void *match, void *aux)
{
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;

	if (uha->reportid != UHIDEV_CLAIM_ALLREPORTID)
		return (UMATCH_NONE);

	if (upd_lookup(uha->uaa->vendor, uha->uaa->product) == NULL)
		return (UMATCH_NONE);

	DPRINTF(("upd: vendor=0x%x, product=0x%x\n", uha->uaa->vendor,
	    uha->uaa->product));

	return (UMATCH_VENDOR_PRODUCT);
}

void
upd_attach(struct device *parent, struct device *self, void *aux)
{
	struct upd_softc	 *sc = (struct upd_softc *)self;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	struct hid_item		  item;
	struct hid_data		 *hdata;
	int			  size;
	void			 *desc;

	sc->sc_hdev.sc_intr = upd_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_num_sensors = 0;

	strlcpy(sc->sc_sensordev.xname, sc->sc_hdev.sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	DPRINTF(("upd: devname=%s sc_nrepid=%d\n", sc->sc_hdev.sc_dev.dv_xname,
	    uha->parent->sc_nrepid));

	uhidev_get_report_desc(uha->parent, &desc, &size);
	hdata = hid_start_parse(desc, size, hid_feature);
	/* lookup for item in our sensors list */
	while (hid_get_item(hdata, &item))
		upd_add_sensor(sc, &item, desc, size);

	hid_end_parse(hdata);
	DPRINTF(("upd: sc_num_sensors=%d\n", sc->sc_num_sensors));

	if (sc->sc_num_sensors > 0) {
		sc->sc_sensortask = sensor_task_register(sc, upd_refresh, 6);
		if (sc->sc_sensortask == NULL) {
			printf(", unable to register update task\n");
			return;
		}
		sensordev_install(&sc->sc_sensordev);
	}

	printf("\n");

	DPRINTF(("upd_attach: complete\n"));
}

int
upd_detach(struct device *self, int flags)
{
	struct upd_softc	*sc = (struct upd_softc *)self;
	struct upd_sensor	*sensor;
	int			 i;

	if (sc->sc_num_sensors <= 0)
		goto finish;

	if (sc->sc_sensortask != NULL) {
		wakeup(&sc->sc_sensortask);
		sensor_task_unregister(sc->sc_sensortask);
	}

	sensordev_deinstall(&sc->sc_sensordev);

	for (i = 0; i < UPD_SENSOR_NUM; i++) {
		sensor = &sc->sc_sensors[i];
		if (!sensor->attached)
			continue;

		sensor_detach(&sc->sc_sensordev, &sensor->sensor);
		DPRINTF(("upd_detach: %s\n", sensor->sensor.desc));
	}

finish:
	DPRINTF(("upd_detach: complete\n"));
	return (0);
}

void
upd_refresh(void *arg)
{
	struct upd_softc	*sc = (struct upd_softc *)arg;
	struct hid_location	*loc;
	struct upd_sensor	*sensor;
	ulong			hdata;
	uint8_t			buf[256];
	int			i, err;

	for (i = 0; i < UPD_SENSOR_NUM; i++) {
		sensor = &sc->sc_sensors[i];
		if (sensor && ! sensor->attached)
			continue;

		loc = &sensor->item.loc;
		sc->sc_hdev.sc_report_id = sensor->item.report_ID;
		memset(buf, 0x0, sizeof(buf));
		err = uhidev_get_report(&sc->sc_hdev, UHID_FEATURE_REPORT, buf,
		    sensor->flen);

		if (err) {
			DPRINTF(("read failure: sens=%02x reportid=%02x err=%d\n", i,
			    sc->sc_hdev.sc_report_id, err));
			continue;
		}

		hdata = hid_get_data(buf + 1, loc);
		switch (i) {
		case UPD_SENSOR_RELCHARGE:
		case UPD_SENSOR_ABSCHARGE:
		case UPD_SENSOR_REMCAPACI:
		case UPD_SENSOR_FULLCHARG:
			if (sc->sc_sensors[UPD_SENSOR_BATTPRESENT].sensor.value)
				hdata *= 1000; /* scale adjust */
			else
				hdata = 0;
			break;
		}

		sensor->sensor.flags &= ~SENSOR_FINVALID;
		sensor->sensor.value = hdata;
		DPRINTF(("%s: %s: hidget data: %d\n",
		    sc->sc_sensordev.xname, upd_usage_table[i].usage_name,
		    hdata));
	}
}

void
upd_add_sensor(struct upd_softc *sc, const struct hid_item *item, void *desc,
    int dsiz)
{
	struct upd_usage_entry	*entry = NULL;
	struct upd_sensor	*sensor = NULL;
	int i;

	for (i = 0; i < UPD_SENSOR_NUM; i++) {
		entry = &upd_usage_table[i];
		if (entry->upd_sid == UPD_SENSOR_UNKNOWN ||
		    entry->usage_pg != HID_GET_USAGE_PAGE(item->usage) ||
		    entry->usage_id != HID_GET_USAGE(item->usage))
			continue;

		sensor = &sc->sc_sensors[i];
		if (sensor && sensor->attached)
			continue;

		/* keep our copy of hid_item */
		memset(&sensor->item, 0x0, sizeof(struct hid_item));
		memcpy(&sensor->item, item, sizeof(struct hid_item));
		sensor->flen = hid_report_size(desc, dsiz, hid_feature,
		    item->report_ID) + 1;
		strlcpy(sensor->sensor.desc, entry->usage_name,
		    sizeof(sensor->sensor.desc));
		sensor->sensor.type = entry->senstype;
		sensor->sensor.flags |= SENSOR_FINVALID;
		sensor->sensor.value = 0;
		sensor_attach(&sc->sc_sensordev, &sensor->sensor);
		sensor->attached = 1;
		sc->sc_num_sensors++;
	}
}

void
upd_intr(struct uhidev *uh, void *p, uint len)
{
	/* noop */
}
