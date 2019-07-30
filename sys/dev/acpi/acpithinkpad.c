/*	$OpenBSD: acpithinkpad.c,v 1.58 2017/08/12 17:33:51 jcs Exp $	*/
/*
 * Copyright (c) 2008 joshua stein <jcs@openbsd.org>
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

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>
#include <dev/wscons/wsconsio.h>

#include <machine/apmvar.h>

#include "audio.h"
#include "wskbd.h"

/* #define ACPITHINKPAD_DEBUG */

#ifdef ACPITHINKPAD_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define	THINKPAD_HKEY_VERSION1		0x0100
#define	THINKPAD_HKEY_VERSION2		0x0200

#define	THINKPAD_CMOS_VOLUME_DOWN	0x00
#define	THINKPAD_CMOS_VOLUME_UP		0x01
#define	THINKPAD_CMOS_VOLUME_MUTE	0x02
#define	THINKPAD_CMOS_BRIGHTNESS_UP	0x04
#define	THINKPAD_CMOS_BRIGHTNESS_DOWN	0x05

#define	THINKPAD_BLUETOOTH_PRESENT	0x01
#define	THINKPAD_BLUETOOTH_ENABLED	0x02

/* wan (not wifi) card */
#define	THINKPAD_WAN_PRESENT		0x01
#define	THINKPAD_WAN_ENABLED		0x02

#define	THINKPAD_BUTTON_FN_F1		0x1001
#define	THINKPAD_BUTTON_LOCK_SCREEN	0x1002
#define	THINKPAD_BUTTON_BATTERY_INFO	0x1003
#define	THINKPAD_BUTTON_SUSPEND		0x1004
#define	THINKPAD_BUTTON_WIRELESS	0x1005
#define	THINKPAD_BUTTON_FN_F6		0x1006
#define	THINKPAD_BUTTON_EXTERNAL_SCREEN	0x1007
#define	THINKPAD_BUTTON_POINTER_SWITCH	0x1008
#define	THINKPAD_BUTTON_EJECT		0x1009
#define	THINKPAD_BUTTON_FN_F11		0x100b
#define	THINKPAD_BUTTON_HIBERNATE	0x100c
#define	THINKPAD_BUTTON_BRIGHTNESS_UP	0x1010
#define	THINKPAD_BUTTON_BRIGHTNESS_DOWN	0x1011
#define	THINKPAD_BUTTON_THINKLIGHT	0x1012
#define	THINKPAD_BUTTON_FN_SPACE	0x1014
#define	THINKPAD_BUTTON_VOLUME_UP	0x1015
#define	THINKPAD_BUTTON_VOLUME_DOWN	0x1016
#define	THINKPAD_BUTTON_VOLUME_MUTE	0x1017
#define	THINKPAD_BUTTON_THINKVANTAGE	0x1018
#define	THINKPAD_BUTTON_BLACK		0x101a
#define	THINKPAD_BUTTON_MICROPHONE_MUTE	0x101b
#define	THINKPAD_KEYLIGHT_CHANGED	0x101c
#define	THINKPAD_BUTTON_CONFIG		0x101d
#define	THINKPAD_BUTTON_FIND		0x101e
#define	THINKPAD_BUTTON_ALL_ACTIVEPROGS	0x101f
#define	THINKPAD_BUTTON_ALL_PROGS	0x1020

#define	THINKPAD_ADAPTIVE_NEXT		0x1101
#define	THINKPAD_ADAPTIVE_QUICK		0x1102
#define	THINKPAD_ADAPTIVE_SNIP		0x1105
#define	THINKPAD_ADAPTIVE_VOICE		0x1108
#define	THINKPAD_ADAPTIVE_GESTURES	0x110a
#define	THINKPAD_ADAPTIVE_SETTINGS	0x110e
#define	THINKPAD_ADAPTIVE_TAB		0x110f
#define	THINKPAD_ADAPTIVE_REFRESH	0x1110
#define	THINKPAD_ADAPTIVE_BACK		0x1111
#define THINKPAD_PORT_REPL_DOCKED	0x4010
#define THINKPAD_PORT_REPL_UNDOCKED	0x4011
#define	THINKPAD_TABLET_DOCKED		0x4012
#define	THINKPAD_TABLET_UNDOCKED	0x4013
#define	THINKPAD_LID_OPEN		0x5001
#define	THINKPAD_LID_CLOSED		0x5002
#define	THINKPAD_TABLET_SCREEN_NORMAL	0x500a
#define	THINKPAD_TABLET_SCREEN_ROTATED	0x5009
#define	THINKPAD_BRIGHTNESS_CHANGED	0x5010
#define	THINKPAD_TABLET_PEN_INSERTED	0x500b
#define	THINKPAD_TABLET_PEN_REMOVED	0x500c
#define	THINKPAD_SWITCH_NUMLOCK		0x6000
#define	THINKPAD_BUTTON_ROTATION_LOCK	0x6020
#define	THINKPAD_THERMAL_TABLE_CHANGED	0x6030
#define	THINKPAD_POWER_CHANGED		0x6040
#define	THINKPAD_BACKLIGHT_CHANGED	0x6050
#define	THINKPAD_BUTTON_FN_TOGGLE       0x6060
#define	THINKPAD_TABLET_SCREEN_CHANGED	0x60c0
#define	THINKPAD_SWITCH_WIRELESS	0x7000

#define THINKPAD_NSENSORS 9
#define THINKPAD_NTEMPSENSORS 8

#define THINKPAD_ECOFFSET_VOLUME	0x30
#define THINKPAD_ECOFFSET_VOLUME_MUTE_MASK 0x40
#define THINKPAD_ECOFFSET_FANLO		0x84
#define THINKPAD_ECOFFSET_FANHI		0x85

#define	THINKPAD_ADAPTIVE_MODE_HOME	1
#define	THINKPAD_ADAPTIVE_MODE_FUNCTION	3

struct acpithinkpad_softc {
	struct device		 sc_dev;

	struct acpiec_softc     *sc_ec;
	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	struct ksensor		 sc_sens[THINKPAD_NSENSORS];
	struct ksensordev	 sc_sensdev;

	uint64_t		 sc_thinklight;
	const char		*sc_thinklight_get;
	const char		*sc_thinklight_set;

	uint64_t		 sc_brightness;
};

extern void acpiec_read(struct acpiec_softc *, u_int8_t, int, u_int8_t *);

int	thinkpad_match(struct device *, void *, void *);
void	thinkpad_attach(struct device *, struct device *, void *);
int	thinkpad_hotkey(struct aml_node *, int, void *);
int	thinkpad_enable_events(struct acpithinkpad_softc *);
int	thinkpad_toggle_bluetooth(struct acpithinkpad_softc *);
int	thinkpad_toggle_wan(struct acpithinkpad_softc *);
int	thinkpad_cmos(struct acpithinkpad_softc *sc, uint8_t);
int	thinkpad_volume_down(struct acpithinkpad_softc *);
int	thinkpad_volume_up(struct acpithinkpad_softc *);
int	thinkpad_volume_mute(struct acpithinkpad_softc *);
int	thinkpad_brightness_up(struct acpithinkpad_softc *);
int	thinkpad_brightness_down(struct acpithinkpad_softc *);
int	thinkpad_adaptive_change(struct acpithinkpad_softc *);
int	thinkpad_activate(struct device *, int);

/* wscons hook functions */
void	thinkpad_get_thinklight(struct acpithinkpad_softc *);
void	thinkpad_set_thinklight(void *, int);
int	thinkpad_get_backlight(struct wskbd_backlight *);
int	thinkpad_set_backlight(struct wskbd_backlight *);
extern int (*wskbd_get_backlight)(struct wskbd_backlight *);
extern int (*wskbd_set_backlight)(struct wskbd_backlight *);
void	thinkpad_get_brightness(struct acpithinkpad_softc *);
void	thinkpad_set_brightness(void *, int);
int	thinkpad_get_param(struct wsdisplay_param *);
int	thinkpad_set_param(struct wsdisplay_param *);
extern int (*ws_get_param)(struct wsdisplay_param *);
extern int (*ws_set_param)(struct wsdisplay_param *);

void    thinkpad_sensor_attach(struct acpithinkpad_softc *sc);
void    thinkpad_sensor_refresh(void *);

#if NAUDIO > 0 && NWSKBD > 0
void thinkpad_attach_deferred(void *);
int thinkpad_get_volume_mute(struct acpithinkpad_softc *);
extern int wskbd_set_mixermute(long, long);
extern int wskbd_set_mixervolume(long, long);
#endif

struct cfattach acpithinkpad_ca = {
	sizeof(struct acpithinkpad_softc), thinkpad_match, thinkpad_attach,
	NULL, thinkpad_activate
};

struct cfdriver acpithinkpad_cd = {
	NULL, "acpithinkpad", DV_DULL
};

const char *acpithinkpad_hids[] = {
	"IBM0068",
	"LEN0068",
	"LEN0268",
	0
};

int
thinkpad_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata *cf = match;
	int64_t	res;

	if (!acpi_matchhids(aa, acpithinkpad_hids, cf->cf_driver->cd_name))
		return (0);

	if (aml_evalinteger((struct acpi_softc *)parent, aa->aaa_node,
	    "MHKV", 0, NULL, &res))
		return (0);

	if (!(res == THINKPAD_HKEY_VERSION1 || res == THINKPAD_HKEY_VERSION2))
		return (0);

	return (1);
}

void
thinkpad_sensor_attach(struct acpithinkpad_softc *sc)
{
	int i;

	if (sc->sc_acpi->sc_ec == NULL)
		return;
	sc->sc_ec = sc->sc_acpi->sc_ec;

	/* Add temperature probes */
	strlcpy(sc->sc_sensdev.xname, DEVNAME(sc),
	    sizeof(sc->sc_sensdev.xname));
	for (i=0; i<THINKPAD_NTEMPSENSORS; i++) {
		sc->sc_sens[i].type = SENSOR_TEMP;
		sensor_attach(&sc->sc_sensdev, &sc->sc_sens[i]);
	}

	/* Add fan probe */
	sc->sc_sens[i].type = SENSOR_FANRPM;
	sensor_attach(&sc->sc_sensdev, &sc->sc_sens[i]);

	sensordev_install(&sc->sc_sensdev);
}

void
thinkpad_sensor_refresh(void *arg)
{
	struct acpithinkpad_softc *sc = arg;
	u_int8_t lo, hi, i;
	int64_t tmp;
	char sname[5];

	/* Refresh sensor readings */
	for (i=0; i<THINKPAD_NTEMPSENSORS; i++) {
		snprintf(sname, sizeof(sname), "TMP%d", i);
		aml_evalinteger(sc->sc_acpi, sc->sc_ec->sc_devnode,
		    sname, 0, 0, &tmp);
		sc->sc_sens[i].value = (tmp * 1000000) + 273150000;
		if (tmp > 127 || tmp < -127)
			sc->sc_sens[i].flags = SENSOR_FINVALID;
	}

	/* Read fan RPM */
	acpiec_read(sc->sc_ec, THINKPAD_ECOFFSET_FANLO, 1, &lo);
	acpiec_read(sc->sc_ec, THINKPAD_ECOFFSET_FANHI, 1, &hi);
	sc->sc_sens[i].value = ((hi << 8L) + lo);
}

void
thinkpad_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpithinkpad_softc *sc = (struct acpithinkpad_softc *)self;
	struct acpi_attach_args	*aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	printf("\n");

#if NAUDIO > 0 && NWSKBD > 0
	/* Defer speaker mute */
	if (thinkpad_get_volume_mute(sc) == 1)
		startuphook_establish(thinkpad_attach_deferred, sc);
#endif

	/* Set event mask to receive everything */
	thinkpad_enable_events(sc);
	thinkpad_sensor_attach(sc);

	/* Check for ThinkLight or keyboard backlight */
	if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "KLCG",
	    0, NULL, &sc->sc_thinklight) == 0) {
		sc->sc_thinklight_get = "KLCG";
		sc->sc_thinklight_set = "KLCS";
		wskbd_get_backlight = thinkpad_get_backlight;
		wskbd_set_backlight = thinkpad_set_backlight;
	} else if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "MLCG",
	    0, NULL, &sc->sc_thinklight) == 0) {
		sc->sc_thinklight_get = "MLCG";
		sc->sc_thinklight_set = "MLCS";
		wskbd_get_backlight = thinkpad_get_backlight;
		wskbd_set_backlight = thinkpad_set_backlight;
	}

	if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "PBLG",
	    0, NULL, &sc->sc_brightness) == 0) {
		ws_get_param = thinkpad_get_param;
		ws_set_param = thinkpad_set_param;
	}

	/* Run thinkpad_hotkey on button presses */
	aml_register_notify(sc->sc_devnode, aa->aaa_dev,
	    thinkpad_hotkey, sc, ACPIDEV_POLL);
}

int
thinkpad_enable_events(struct acpithinkpad_softc *sc)
{
	struct aml_value arg, args[2];
	int64_t	mask;
	int i;

	/* Get the supported event mask */
	if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "MHKA",
	    0, NULL, &mask)) {
		printf("%s: no MHKA\n", DEVNAME(sc));
		return (1);
	}

	/* Update hotkey mask */
	bzero(args, sizeof(args));
	args[0].type = args[1].type = AML_OBJTYPE_INTEGER;
	for (i = 0; i < 32; i++) {
		args[0].v_integer = i + 1;
		args[1].v_integer = (((1 << i) & mask) != 0);

		if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "MHKM",
		    2, args, NULL)) {
			printf("%s: couldn't toggle MHKM\n", DEVNAME(sc));
			return (1);
		}
	}

	/* Enable hotkeys */
	bzero(&arg, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = 1;
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "MHKC",
	    1, &arg, NULL)) {
		printf("%s: couldn't enable hotkeys\n", DEVNAME(sc));
		return (1);
	}

	return (0);
}

int
thinkpad_hotkey(struct aml_node *node, int notify_type, void *arg)
{
	struct acpithinkpad_softc *sc = arg;
	int64_t	event;

	if (notify_type == 0x00) {
		/* Poll sensors */
		thinkpad_sensor_refresh(sc);
		return (0);
	}

	if (notify_type != 0x80)
		return (1);

	for (;;) {
		if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "MHKP",
		    0, NULL, &event))
			break;
		if (event == 0)
			break;

		switch (event) {
		case THINKPAD_BUTTON_BRIGHTNESS_UP:
			thinkpad_brightness_up(sc);
			break;
		case THINKPAD_BUTTON_BRIGHTNESS_DOWN:
			thinkpad_brightness_down(sc);
			break;
		case THINKPAD_BUTTON_WIRELESS:
			thinkpad_toggle_bluetooth(sc);
			break;
		case THINKPAD_BUTTON_SUSPEND:
#ifndef SMALL_KERNEL
			if (acpi_record_event(sc->sc_acpi, APM_USER_SUSPEND_REQ))
				acpi_addtask(sc->sc_acpi, acpi_sleep_task, 
				    sc->sc_acpi, ACPI_SLEEP_SUSPEND);
#endif
			break;
		case THINKPAD_BUTTON_VOLUME_MUTE:
			thinkpad_volume_mute(sc);
			break;
		case THINKPAD_BUTTON_VOLUME_DOWN:
			thinkpad_volume_down(sc);
			break;
		case THINKPAD_BUTTON_VOLUME_UP:
			thinkpad_volume_up(sc);
			break;
		case THINKPAD_BUTTON_MICROPHONE_MUTE:
#if NAUDIO > 0 && NWSKBD > 0
			wskbd_set_mixervolume(0, 0);
#endif
			break;
		case THINKPAD_BUTTON_HIBERNATE:
#if defined(HIBERNATE) && !defined(SMALL_KERNEL)
			if (acpi_record_event(sc->sc_acpi, APM_USER_HIBERNATE_REQ))
				acpi_addtask(sc->sc_acpi, acpi_sleep_task, 
				    sc->sc_acpi, ACPI_SLEEP_HIBERNATE);
#endif
			break;
		case THINKPAD_BUTTON_THINKLIGHT:
			thinkpad_get_thinklight(sc);
			break;
		case THINKPAD_ADAPTIVE_NEXT:
		case THINKPAD_ADAPTIVE_QUICK:
			thinkpad_adaptive_change(sc);
			break;
		case THINKPAD_BACKLIGHT_CHANGED:
			thinkpad_get_brightness(sc);
			break;
		default:
			/* unknown or boring event */
			DPRINTF(("%s: unhandled event 0x%03llx\n", DEVNAME(sc),
			    event));
			break;
		}
	}

	return (0);
}

int
thinkpad_toggle_bluetooth(struct acpithinkpad_softc *sc)
{
	struct aml_value arg;
	int64_t	bluetooth;

	if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "GBDC",
	    0, NULL, &bluetooth))
		return (1);

	if (!(bluetooth & THINKPAD_BLUETOOTH_PRESENT))
		return (1);

	bzero(&arg, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = bluetooth ^ THINKPAD_BLUETOOTH_ENABLED;
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "SBDC",
	    1, &arg, NULL)) {
		printf("%s: couldn't toggle bluetooth\n", DEVNAME(sc));
		return (1);
	}

	return (0);
}

int
thinkpad_toggle_wan(struct acpithinkpad_softc *sc)
{
	struct aml_value arg;
	int64_t wan;

	if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "GWAN",
	    0, NULL, &wan))
		return (1);

	if (!(wan & THINKPAD_WAN_PRESENT))
		return (1);

	bzero(&arg, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = wan ^ THINKPAD_WAN_ENABLED;
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "SWAN",
	    1, &arg, NULL)) {
		printf("%s: couldn't toggle wan\n", DEVNAME(sc));
		return (1);
	}

	return (0);
}

int
thinkpad_cmos(struct acpithinkpad_softc *sc, uint8_t cmd)
{
	struct aml_value arg;

	bzero(&arg, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = cmd;
	aml_evalname(sc->sc_acpi, sc->sc_devnode, "\\UCMS", 1, &arg, NULL);
	return (0);
}

int
thinkpad_volume_down(struct acpithinkpad_softc *sc)
{
	return (thinkpad_cmos(sc, THINKPAD_CMOS_VOLUME_DOWN));
}

int
thinkpad_volume_up(struct acpithinkpad_softc *sc)
{
	return (thinkpad_cmos(sc, THINKPAD_CMOS_VOLUME_UP));
}

int
thinkpad_volume_mute(struct acpithinkpad_softc *sc)
{
	return (thinkpad_cmos(sc, THINKPAD_CMOS_VOLUME_MUTE));
}

int
thinkpad_brightness_up(struct acpithinkpad_softc *sc)
{
	return (thinkpad_cmos(sc, THINKPAD_CMOS_BRIGHTNESS_UP));
}

int
thinkpad_brightness_down(struct acpithinkpad_softc *sc)
{
	return (thinkpad_cmos(sc, THINKPAD_CMOS_BRIGHTNESS_DOWN));
}

int
thinkpad_adaptive_change(struct acpithinkpad_softc *sc)
{
	struct aml_value arg;
	int64_t	mode;

	if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "GTRW",
	    0, NULL, &mode)) {
		printf("%s: couldn't get adaptive keyboard mode\n", DEVNAME(sc));
		return (1);
	}

	bzero(&arg, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;

	if (mode == THINKPAD_ADAPTIVE_MODE_FUNCTION)
		arg.v_integer = THINKPAD_ADAPTIVE_MODE_HOME;
	else
		arg.v_integer = THINKPAD_ADAPTIVE_MODE_FUNCTION;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "STRW",
	    1, &arg, NULL)) {
		printf("%s: couldn't set adaptive keyboard mode\n", DEVNAME(sc));
		return (1);
	}

	return (0);
}

int
thinkpad_activate(struct device *self, int act)
{

	struct acpithinkpad_softc *sc = (struct acpithinkpad_softc *)self;
	int64_t res;
#if NAUDIO > 0 && NWSKBD > 0
	int mute;
#endif

	switch (act) {
	case DVACT_WAKEUP:
		if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "GTRW",
		    0, NULL, &res) == 0)
			thinkpad_adaptive_change(sc);
#if NAUDIO > 0 && NWSKBD > 0
		mute = thinkpad_get_volume_mute(sc);
		if (mute != -1)
			wskbd_set_mixermute(mute, 1);
#endif
		break;
	}
	return (0);
}

void
thinkpad_get_thinklight(struct acpithinkpad_softc *sc)
{
	if (sc->sc_thinklight_get)
		aml_evalinteger(sc->sc_acpi, sc->sc_devnode,
		    sc->sc_thinklight_get, 0, NULL, &sc->sc_thinklight);
}

void
thinkpad_set_thinklight(void *arg0, int arg1)
{
	struct acpithinkpad_softc *sc = arg0;
	struct aml_value arg;

	memset(&arg, 0, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = sc->sc_thinklight & 0x0f;
	aml_evalname(sc->sc_acpi, sc->sc_devnode,
	    sc->sc_thinklight_set, 1, &arg, NULL);
}

int
thinkpad_get_backlight(struct wskbd_backlight *kbl)
{
	struct acpithinkpad_softc *sc = acpithinkpad_cd.cd_devs[0];

	KASSERT(sc != NULL);

	kbl->min = 0;
	kbl->max = (sc->sc_thinklight >> 8) & 0x0f;
	kbl->curval = sc->sc_thinklight & 0x0f;

	if (kbl->max == 0)
		return (ENOTTY);

	return 0;
}

int
thinkpad_set_backlight(struct wskbd_backlight *kbl)
{
	struct acpithinkpad_softc *sc = acpithinkpad_cd.cd_devs[0];
	int maxval;

	KASSERT(sc != NULL);

	maxval = (sc->sc_thinklight >> 8) & 0x0f;

	if (maxval == 0)
		return (ENOTTY);

	if (kbl->curval > maxval)
		return EINVAL;

	sc->sc_thinklight &= ~0xff;
	sc->sc_thinklight |= kbl->curval;
	acpi_addtask(sc->sc_acpi, thinkpad_set_thinklight, sc, 0);
	acpi_wakeup(sc->sc_acpi);
	return 0;
}

void
thinkpad_get_brightness(struct acpithinkpad_softc *sc)
{
	aml_evalinteger(sc->sc_acpi, sc->sc_devnode,
	    "PBLG", 0, NULL, &sc->sc_brightness);
}

void
thinkpad_set_brightness(void *arg0, int arg1)
{
	struct acpithinkpad_softc *sc = arg0;
	struct aml_value arg;

	memset(&arg, 0, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = sc->sc_brightness & 0xff;
	aml_evalname(sc->sc_acpi, sc->sc_devnode,
	    "PBLS", 1, &arg, NULL);
}

int
thinkpad_get_param(struct wsdisplay_param *dp)
{
	struct acpithinkpad_softc *sc = acpithinkpad_cd.cd_devs[0];

	if (sc == NULL)
		return -1;

	switch (dp->param) {
	case WSDISPLAYIO_PARAM_BRIGHTNESS:
		dp->min = 0;
		dp->max = (sc->sc_brightness >> 8) & 0xff;
		dp->curval = sc->sc_brightness & 0xff;
		return 0;
	default:
		return -1;
	}
}

int
thinkpad_set_param(struct wsdisplay_param *dp)
{
	struct acpithinkpad_softc *sc = acpithinkpad_cd.cd_devs[0];
	int maxval;

	if (sc == NULL)
		return -1;

	maxval = (sc->sc_brightness >> 8) & 0xff;

	switch (dp->param) {
	case WSDISPLAYIO_PARAM_BRIGHTNESS:
		if (dp->curval < 0)
			dp->curval = 0;
		if (dp->curval > maxval)
			dp->curval = maxval;
		sc->sc_brightness &= ~0xff;
		sc->sc_brightness |= dp->curval;
		acpi_addtask(sc->sc_acpi, thinkpad_set_brightness, sc, 0);
		acpi_wakeup(sc->sc_acpi);
		return 0;
	default:
		return -1;
	}
}

#if NAUDIO > 0 && NWSKBD > 0
void
thinkpad_attach_deferred(void *v __unused)
{
	wskbd_set_mixermute(1, 1);
}

int
thinkpad_get_volume_mute(struct acpithinkpad_softc *sc)
{
	u_int8_t vol = 0;

	if (sc->sc_acpi->sc_ec == NULL)
		return (-1);

	acpiec_read(sc->sc_acpi->sc_ec, THINKPAD_ECOFFSET_VOLUME, 1, &vol);
	return ((vol & THINKPAD_ECOFFSET_VOLUME_MUTE_MASK) ==
	    THINKPAD_ECOFFSET_VOLUME_MUTE_MASK);
}
#endif
