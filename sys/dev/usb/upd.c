/*	$OpenBSD: upd.c,v 1.6 2014/04/07 21:54:13 andre Exp $ */

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

struct upd_usage_entry {
	uint8_t			usage_pg;
	uint8_t			usage_id;
	enum sensor_type	senstype;
	char			*usage_name; /* sensor string */
};

static struct upd_usage_entry upd_usage_table[] = {
	{ HUP_BATTERY,	HUB_REL_STATEOF_CHARGE,
	    SENSOR_PERCENT,	 "RelativeStateOfCharge" },
	{ HUP_BATTERY,	HUB_ABS_STATEOF_CHARGE,
	    SENSOR_PERCENT,	 "AbsoluteStateOfCharge" },
	{ HUP_BATTERY,	HUB_REM_CAPACITY,
	    SENSOR_PERCENT,	 "RemainingCapacity" },
	{ HUP_BATTERY,	HUB_FULLCHARGE_CAPACITY,
	    SENSOR_PERCENT,	 "FullChargeCapacity" },
	{ HUP_BATTERY,	HUB_CHARGING,
	    SENSOR_INDICATOR,	 "Charging" },
	{ HUP_BATTERY,	HUB_DISCHARGING,
	    SENSOR_INDICATOR,	 "Discharging" },
	{ HUP_BATTERY,	HUB_BATTERY_PRESENT,
	    SENSOR_INDICATOR,	 "BatteryPresent" },
	{ HUP_POWER,	HUP_SHUTDOWN_IMMINENT,
	    SENSOR_INDICATOR,	 "ShutdownImminent" },
	{ HUP_BATTERY,	HUB_AC_PRESENT,
	    SENSOR_INDICATOR,	 "ACPresent" },
	{ HUP_BATTERY,	HUB_ATRATE_TIMETOFULL,
	    SENSOR_TIMEDELTA,	 "AtRateTimeToFull" }
};

struct upd_report {
	size_t		size;
	int		enabled;
};

struct upd_sensor {
	struct ksensor		ksensor;
	struct hid_item		hitem;
	int			attached;
};

struct upd_softc {
	struct uhidev		 sc_hdev;
	int			 sc_num_sensors;
	u_int			 sc_max_repid;
	u_int			 sc_max_sensors;

	/* sensor framework */
	struct ksensordev	 sc_sensordev;
	struct sensor_task	*sc_sensortask;
	struct upd_report	*sc_reports;
	struct upd_sensor	*sc_sensors;
};

static const struct usb_devno upd_devs[] = {
	{ USB_VENDOR_APC, USB_PRODUCT_APC_UPS },
	{ USB_VENDOR_APC, USB_PRODUCT_APC_UPS5G },
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C100 },
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C1100 },
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C120 },
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C1250EITWRK },
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C1500EITWRK },
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C550AVR },
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C800 },
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C900 },
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6H375 },
	{ USB_VENDOR_CYBERPOWER, USB_PRODUCT_CYBERPOWER_1500 },
	{ USB_VENDOR_CYBERPOWER, USB_PRODUCT_CYBERPOWER_OR2200 },
	{ USB_VENDOR_CYBERPOWER, USB_PRODUCT_CYBERPOWER_UPS },
	{ USB_VENDOR_DELL2, USB_PRODUCT_DELL2_UPS },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_R1500G2 },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_RT2200 },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_T1000 },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_T1500 },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_T750 },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_T750G2 },
	{ USB_VENDOR_IDOWELL, USB_PRODUCT_IDOWELL_IDOWELL },
	{ USB_VENDOR_LIEBERT, USB_PRODUCT_LIEBERT_UPS },
	{ USB_VENDOR_LIEBERT2, USB_PRODUCT_LIEBERT2_PSA },
	{ USB_VENDOR_MGE, USB_PRODUCT_MGE_UPS1 },
	{ USB_VENDOR_MGE, USB_PRODUCT_MGE_UPS2 },
	{ USB_VENDOR_OMRON, USB_PRODUCT_OMRON_BX35F },
	{ USB_VENDOR_OMRON, USB_PRODUCT_OMRON_BX50F },
	{ USB_VENDOR_OMRON, USB_PRODUCT_OMRON_BY35S }
};
#define upd_lookup(v, p) usb_lookup(upd_devs, v, p)

int  upd_match(struct device *, void *, void *);
void upd_attach(struct device *, struct device *, void *);
int  upd_detach(struct device *, int);

void upd_refresh(void *);
void upd_update_sensors(struct upd_softc *, uint8_t *, int);
void upd_intr(struct uhidev *, void *, uint);
struct upd_usage_entry *upd_lookup_usage_entry(const struct hid_item *);
struct upd_sensor *upd_lookup_sensor(struct upd_softc *, int, int);

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
	int			  size;
	void			 *desc;
	struct hid_data		 *hdata;
	struct hid_item		  item;
	int			  ret = UMATCH_NONE;

	if (uha->reportid != UHIDEV_CLAIM_ALLREPORTID)
		return (ret);

	if (upd_lookup(uha->uaa->vendor, uha->uaa->product) == NULL)
		return (ret);

	DPRINTF(("upd: vendor=0x%04x, product=0x%04x\n", uha->uaa->vendor,
	    uha->uaa->product));

	/*
	 * look for at least one sensor of our table, otherwise do not attach
	 */
	uhidev_get_report_desc(uha->parent, &desc, &size);
	for (hdata = hid_start_parse(desc, size, hid_feature);
	     hid_get_item(hdata, &item); ) {
		if (upd_lookup_usage_entry(&item) != NULL) {
			ret = UMATCH_VENDOR_PRODUCT;
			break;
		}
	}
	hid_end_parse(hdata);

	return (ret);
}

void
upd_attach(struct device *parent, struct device *self, void *aux)
{
	struct upd_softc	 *sc = (struct upd_softc *)self;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	struct hid_item		  item;
	struct hid_data		 *hdata;
	struct upd_usage_entry	 *entry;
	struct upd_sensor	 *sensor;
	int			  size;
	void			 *desc;

	sc->sc_hdev.sc_intr = upd_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_reports = NULL;
	sc->sc_sensors = NULL;
	sc->sc_max_sensors = nitems(upd_usage_table);

	strlcpy(sc->sc_sensordev.xname, sc->sc_hdev.sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	sc->sc_max_repid = uha->parent->sc_nrepid;
	DPRINTF(("\nupd: devname=%s sc_max_repid=%d\n",
	    sc->sc_hdev.sc_dev.dv_xname, sc->sc_max_repid));

	sc->sc_reports = malloc(sc->sc_max_repid * sizeof(struct upd_report),
	    M_USBDEV, M_WAITOK | M_ZERO);
	size = sc->sc_max_sensors * sizeof(struct upd_sensor);
	sc->sc_sensors = malloc(size, M_USBDEV, M_WAITOK | M_ZERO);
	sc->sc_num_sensors = 0;
	uhidev_get_report_desc(uha->parent, &desc, &size);
	for (hdata = hid_start_parse(desc, size, hid_feature);
	     hid_get_item(hdata, &item) &&
	     sc->sc_num_sensors < sc->sc_max_sensors; ) {
		DPRINTF(("upd: repid=%d\n", item.report_ID));
		if (item.kind != hid_feature ||
		    item.report_ID < 0)
			continue;

		if ((entry = upd_lookup_usage_entry(&item)) == NULL)
			continue;

		/* filter repeated usages, avoid duplicated sensors */
		sensor = upd_lookup_sensor(sc, entry->usage_pg,
		    entry->usage_id);
		if (sensor && sensor->attached)
			continue;

		sensor = &sc->sc_sensors[sc->sc_num_sensors];
		/* keep our copy of hid_item */
		memcpy(&sensor->hitem, &item, sizeof(struct hid_item));
		strlcpy(sensor->ksensor.desc, entry->usage_name,
		    sizeof(sensor->ksensor.desc));
		sensor->ksensor.type = entry->senstype;
		sensor->ksensor.flags |= SENSOR_FINVALID;
		sensor->ksensor.status = SENSOR_S_UNKNOWN;
		sensor->ksensor.value = 0;
		sensor_attach(&sc->sc_sensordev, &sensor->ksensor);
		sensor->attached = 1;
		sc->sc_num_sensors++;

		if (item.report_ID >= sc->sc_max_repid ||
		    sc->sc_reports[item.report_ID].enabled)
			continue;

		sc->sc_reports[item.report_ID].size = hid_report_size(desc,
		    size, item.kind, item.report_ID);
		sc->sc_reports[item.report_ID].enabled = 1;
	}
	hid_end_parse(hdata);
	DPRINTF(("upd: sc_num_sensors=%d\n", sc->sc_num_sensors));

	sc->sc_sensortask = sensor_task_register(sc, upd_refresh, 6);
	if (sc->sc_sensortask == NULL) {
		printf(", unable to register update task\n");
		return;
	}
	sensordev_install(&sc->sc_sensordev);

	printf("\n");

	DPRINTF(("upd_attach: complete\n"));
}

int
upd_detach(struct device *self, int flags)
{
	struct upd_softc	*sc = (struct upd_softc *)self;
	struct upd_sensor	*sensor;
	int			 i;

	if (sc->sc_sensortask != NULL) {
		wakeup(&sc->sc_sensortask);
		sensor_task_unregister(sc->sc_sensortask);
	}

	sensordev_deinstall(&sc->sc_sensordev);

	for (i = 0; i < sc->sc_num_sensors; i++) {
		sensor = &sc->sc_sensors[i];
		if (sensor->attached)
			sensor_detach(&sc->sc_sensordev, &sensor->ksensor);
		DPRINTF(("upd_detach: %s\n", sensor->ksensor.desc));
	}

	free(sc->sc_reports, M_USBDEV);
	free(sc->sc_sensors, M_USBDEV);
	DPRINTF(("upd_detach: complete\n"));
	return (0);
}

void
upd_refresh(void *arg)
{
	struct upd_softc	*sc = (struct upd_softc *)arg;
	struct upd_report	*report;
	uint8_t			buf[256];
	int			repid, err;

	for (repid = 0; repid < sc->sc_max_repid; repid++) {
		report = &sc->sc_reports[repid];
		if (!report->enabled)
			continue;

		memset(buf, 0x0, sizeof(buf));
		sc->sc_hdev.sc_report_id = repid;
		err = uhidev_get_report(&sc->sc_hdev, UHID_FEATURE_REPORT, buf,
		    report->size + 1); /*
					* cheating with an extra byte on size
					* here, hid_report_size() * is
					* incorrectly telling us report lengths
					* are 1 byte smaller
					*/
		if (err) {
			DPRINTF(("read failure: reportid=%02x err=%d\n", repid,
			    err));
			continue;
		}

		upd_update_sensors(sc, buf, repid);
	}
}

struct upd_usage_entry *
upd_lookup_usage_entry(const struct hid_item *hitem)
{
	struct upd_usage_entry	*entry = NULL;
	int			 i;

	for (i = 0; i < nitems(upd_usage_table); i++) {
		entry = &upd_usage_table[i];
		if (entry->usage_pg == HID_GET_USAGE_PAGE(hitem->usage) &&
		    entry->usage_id == HID_GET_USAGE(hitem->usage))
			return (entry);
	}
	return (NULL);
}

struct upd_sensor *
upd_lookup_sensor(struct upd_softc *sc, int page, int usage)
{
	struct upd_sensor	*sensor = NULL;
	int			 i;

	for (i = 0; i < sc->sc_num_sensors; i++) {
		sensor = &sc->sc_sensors[i];
		if (page == HID_GET_USAGE_PAGE(sensor->hitem.usage) &&
		    usage == HID_GET_USAGE(sensor->hitem.usage))
			return (sensor);
	}
	return (NULL);
}

void
upd_update_sensors(struct upd_softc *sc, uint8_t *buf, int repid)
{
	struct upd_sensor	*sensor;
	struct hid_location	*loc;
	ulong			hdata, batpres;
	ulong 			adjust;
	int			i;

	sensor = upd_lookup_sensor(sc, HUP_BATTERY, HUB_BATTERY_PRESENT);
	batpres = sensor ? sensor->ksensor.value : -1;

	for (i = 0; i < sc->sc_num_sensors; i++) {
		sensor = &sc->sc_sensors[i];
		if (!(sensor->hitem.report_ID == repid && sensor->attached))
			continue;

		/* invalidate battery dependent sensors */
		if (HID_GET_USAGE_PAGE(sensor->hitem.usage) == HUP_BATTERY &&
		    batpres <= 0) {
			/* exception to the battery sensor itself */
			if (HID_GET_USAGE(sensor->hitem.usage) !=
			    HUB_BATTERY_PRESENT) {
				sensor->ksensor.status = SENSOR_S_UNKNOWN;
				sensor->ksensor.flags |= SENSOR_FINVALID;
				continue;
			}
		}

		switch (HID_GET_USAGE(sensor->hitem.usage)) {
		case HUB_REL_STATEOF_CHARGE:
		case HUB_ABS_STATEOF_CHARGE:
		case HUB_REM_CAPACITY:
		case HUB_FULLCHARGE_CAPACITY:
			adjust = 1000; /* scale adjust */
			break;
		default:
			adjust = 1; /* no scale adjust */
			break;
		}

		loc = &sensor->hitem.loc;
		/* first byte which is the report id */
		hdata = hid_get_data(buf + 1, loc);

		sensor->ksensor.value = hdata * adjust;
		sensor->ksensor.status = SENSOR_S_OK;
		sensor->ksensor.flags &= ~SENSOR_FINVALID;
		DPRINTF(("%s: hidget data: %d\n",
		    sc->sc_sensordev.xname, hdata));
	}
}


void
upd_intr(struct uhidev *uh, void *p, uint len)
{
	/* noop */
}
