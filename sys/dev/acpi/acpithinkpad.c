/* $OpenBSD: acpithinkpad.c,v 1.11 2008/06/11 04:42:09 marco Exp $ */
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

#define	THINKPAD_HKEY_VERSION		0x0100

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

/* type 1 events */
#define	THINKPAD_BUTTON_FN_F1		0x001
#define	THINKPAD_BUTTON_LOCK_SCREEN	0x002
#define	THINKPAD_BUTTON_BATTERY_INFO	0x003
#define	THINKPAD_BUTTON_SUSPEND		0x004
#define	THINKPAD_BUTTON_WIRELESS	0x005
#define	THINKPAD_BUTTON_FN_F6		0x006
#define	THINKPAD_BUTTON_EXTERNAL_SCREEN	0x007
#define	THINKPAD_BUTTON_POINTER_SWITCH	0x008
#define	THINKPAD_BUTTON_EJECT		0x009
#define	THINKPAD_BUTTON_BRIGHTNESS_UP	0x010
#define	THINKPAD_BUTTON_BRIGHTNESS_DOWN	0x011
#define	THINKPAD_BUTTON_THINKLIGHT	0x012
#define	THINKPAD_BUTTON_FN_SPACE	0x014
#define	THINKPAD_BUTTON_VOLUME_UP	0x015
#define	THINKPAD_BUTTON_VOLUME_DOWN	0x016
#define	THINKPAD_BUTTON_VOLUME_MUTE	0x017
#define	THINKPAD_BUTTON_THINKVANTAGE	0x018
#define	THINKPAD_BUTTON_FN_F11		0x00b
#define	THINKPAD_BUTTON_HIBERNATE	0x00c

/* type 5 events */
#define	THINKPAD_LID_OPEN		0x001
#define	THINKPAD_LID_CLOSED		0x002
#define	THINKPAD_TABLET_SCREEN_NORMAL	0x00a
#define	THINKPAD_TABLET_SCREEN_ROTATED	0x009
#define	THINKPAD_BRIGHTNESS_CHANGED	0x010
#define	THINKPAD_TABLET_PEN_INSERTED	0x00b
#define	THINKPAD_TABLET_PEN_REMOVED	0x00c

/* type 7 events */
#define	THINKPAD_SWITCH_WIRELESS	0x000

struct acpithinkpad_softc {
	struct device		sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;
};

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

struct cfattach acpithinkpad_ca = {
	sizeof(struct acpithinkpad_softc), thinkpad_match, thinkpad_attach
};

struct cfdriver acpithinkpad_cd = {
	NULL, "acpithinkpad", DV_DULL
};

int
thinkpad_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata		*cf = match;
	struct aml_value	res;
	int			rv = 0;

	if (aa->aaa_name == NULL ||
	    strcmp(aa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aa->aaa_table != NULL)
		return (0);

	if (aml_evalname((struct acpi_softc *)parent, aa->aaa_node,
	    "MHKV", 0, NULL, &res))
		return (0);

	if (aml_val2int(&res) == THINKPAD_HKEY_VERSION)
		rv = 1;

	aml_freevalue(&res);
	return (rv);
}

void
thinkpad_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpithinkpad_softc *sc = (struct acpithinkpad_softc *)self;
	struct acpi_attach_args	*aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	printf("\n");

	/* set event mask to receive everything */
	thinkpad_enable_events(sc);

	/* run thinkpad_hotkey on button presses */
	aml_register_notify(sc->sc_devnode, aa->aaa_dev,
	    thinkpad_hotkey, sc, ACPIDEV_NOPOLL);
}

int
thinkpad_enable_events(struct acpithinkpad_softc *sc)
{
	struct aml_value	res, arg, args[2];
	int64_t			mask;
	int			i, rv = 1;

	/* get the supported event mask */
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "MHKA", 0, NULL, &res)) {
		printf("%s: no MHKA\n", DEVNAME(sc));
		goto fail;
	}
	mask = aml_val2int(&res);
	aml_freevalue(&res);

	/* update hotkey mask */
	bzero(args, sizeof(args));
	args[0].type = args[1].type = AML_OBJTYPE_INTEGER;
	for (i = 0; i < 32; i++) {
		args[0].v_integer = i + 1;
		args[1].v_integer = (((1 << i) & mask) != 0);

		if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "MHKM", 2, args,
		    NULL)) {
			printf("%s: couldn't toggle MHKM\n", DEVNAME(sc));
			goto fail;
		}
	}

	/* enable hotkeys */
	bzero(&arg, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = 1;
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "MHKC", 1, &arg, NULL)) {
		printf("%s: couldn't enable hotkeys\n", DEVNAME(sc));
		goto fail;
	}

	rv = 0;
fail:
	return (rv);
}

int
thinkpad_hotkey(struct aml_node *node, int notify_type, void *arg)
{
	struct acpithinkpad_softc *sc = arg;
	struct aml_value	res;
	int			val, type, event, handled, rv = 1, tot = 0;

	if (notify_type != 0x80)
		goto fail;

	for (;;) {
		if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "MHKP", 0, NULL,
		    &res))
			goto done;
		val = aml_val2int(&res);
		aml_freevalue(&res);
		if (val == 0)
			goto done;

		type = (val & 0xf000) >> 12;
		event = val & 0x0fff;
		handled = 0;

		switch (type) {
		case 1:
			switch (event) {
			case THINKPAD_BUTTON_BRIGHTNESS_UP:
				thinkpad_brightness_up(sc);
				handled = 1;
				break;
			case THINKPAD_BUTTON_BRIGHTNESS_DOWN:
				thinkpad_brightness_down(sc);
				handled = 1;
				break;
			case THINKPAD_BUTTON_WIRELESS:
				thinkpad_toggle_bluetooth(sc);
				handled = 1;
				break;
			case THINKPAD_BUTTON_SUSPEND:
				handled = 1;
				/* 
				acpi_enter_sleep_state(sc->sc_acpi,
				    ACPI_STATE_S3);
				*/
				break;
			case THINKPAD_BUTTON_HIBERNATE:
			case THINKPAD_BUTTON_FN_F1:
			case THINKPAD_BUTTON_LOCK_SCREEN:
			case THINKPAD_BUTTON_BATTERY_INFO:
			case THINKPAD_BUTTON_FN_F6:
			case THINKPAD_BUTTON_EXTERNAL_SCREEN:
			case THINKPAD_BUTTON_POINTER_SWITCH:
			case THINKPAD_BUTTON_EJECT:
			case THINKPAD_BUTTON_THINKLIGHT:
			case THINKPAD_BUTTON_FN_SPACE:
				handled = 1;
				break;
			case THINKPAD_BUTTON_VOLUME_DOWN:
				thinkpad_volume_down(sc);
				handled = 1;
				break;
			case THINKPAD_BUTTON_VOLUME_UP:
				thinkpad_volume_up(sc);
				handled = 1;
				break;
			case THINKPAD_BUTTON_VOLUME_MUTE:
				thinkpad_volume_mute(sc);
				handled = 1;
				break;
			case THINKPAD_BUTTON_THINKVANTAGE:
			case THINKPAD_BUTTON_FN_F11:
				handled = 1;
				break;
			}
			break;
		case 5:
			switch (event) {
			case THINKPAD_LID_OPEN:
			case THINKPAD_LID_CLOSED:
			case THINKPAD_TABLET_SCREEN_NORMAL:
			case THINKPAD_TABLET_SCREEN_ROTATED:
			case THINKPAD_BRIGHTNESS_CHANGED:
			case THINKPAD_TABLET_PEN_INSERTED:
			case THINKPAD_TABLET_PEN_REMOVED:
				handled = 1;
				break;
			}
			break;
		case 7:
			switch (event) {
			case THINKPAD_SWITCH_WIRELESS:
				handled = 1;
				break;
			}
			break;
		}

		if (handled)
			tot++;
		else
			printf("%s: unknown type %d event 0x%03x\n",
			    DEVNAME(sc), type, event);
	}
done:
	if (tot)
		rv = 0;
fail:
	return (rv);
}

int
thinkpad_toggle_bluetooth(struct acpithinkpad_softc *sc)
{
	struct aml_value	res, arg;
	int			bluetooth, rv = 1;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "GBDC", 0, NULL, &res))
		goto fail;

	bluetooth = aml_val2int(&res);
	aml_freevalue(&res);

	if (!(bluetooth & THINKPAD_BLUETOOTH_PRESENT))
		goto fail;

	bzero(&arg, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = bluetooth ^= THINKPAD_BLUETOOTH_ENABLED;
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "SBDC", 1, &arg, NULL)) {
		printf("%s: couldn't toggle bluetooth\n", DEVNAME(sc));
		goto fail;
	}

	rv = 0;
fail:
	return (rv);
}

int
thinkpad_toggle_wan(struct acpithinkpad_softc *sc)
{
	struct aml_value	res, arg;
	int			wan, rv = 1;;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "GWAN", 0, NULL, &res))
		goto fail;

	wan = aml_val2int(&res);
	aml_freevalue(&res);

	if (!(wan & THINKPAD_WAN_PRESENT))
		goto fail;

	bzero(&arg, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = (wan ^= THINKPAD_WAN_ENABLED);
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "SWAN", 1, &arg, NULL)) {
		printf("%s: couldn't toggle wan\n", DEVNAME(sc));
		goto fail;
	}

	rv = 0;
fail:
	return (rv);
}

int
thinkpad_cmos(struct acpithinkpad_softc *sc, uint8_t cmd)
{
	struct aml_value	arg;

	bzero(&arg, sizeof(arg));
	arg.type = AML_OBJTYPE_INTEGER;
	arg.v_integer = cmd;
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "\\UCMS", 1, &arg,
	    NULL)) {
		printf("%s: cmos command 0x%x failed\n", DEVNAME(sc), cmd);
		return (1);
	}

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
