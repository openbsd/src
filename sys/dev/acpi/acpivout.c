/*	$OpenBSD: acpivout.c,v 1.8 2011/04/06 21:16:13 martynas Exp $	*/
/*
 * Copyright (c) 2009 Paul Irofti <pirofti@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/wscons/wsconsio.h>

int	acpivout_match(struct device *, void *, void *);
void	acpivout_attach(struct device *, struct device *, void *);
int	acpivout_notify(struct aml_node *, int, void *);

#ifdef ACPIVIDEO_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

/* Notifications for Output Devices */
#define NOTIFY_BRIGHTNESS_CYCLE		0x85
#define NOTIFY_BRIGHTNESS_UP		0x86
#define NOTIFY_BRIGHTNESS_DOWN		0x87
#define NOTIFY_BRIGHTNESS_ZERO		0x88
#define NOTIFY_DISPLAY_OFF		0x89

struct acpivout_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	int	*sc_bcl;
	size_t	sc_bcl_len;
};

void	acpivout_brightness_cycle(struct acpivout_softc *);
void	acpivout_brightness_up(struct acpivout_softc *);
void	acpivout_brightness_down(struct acpivout_softc *);
void	acpivout_brightness_zero(struct acpivout_softc *);
int	acpivout_get_brightness(struct acpivout_softc *);
int	acpivout_find_brightness(struct acpivout_softc *, int);
void	acpivout_set_brightness(struct acpivout_softc *, int);
void	acpivout_get_bcl(struct acpivout_softc *);

/* wconsole hook functions */
int	acpivout_get_param(struct wsdisplay_param *);
int	acpivout_set_param(struct wsdisplay_param *);

extern int (*ws_get_param)(struct wsdisplay_param *);
extern int (*ws_set_param)(struct wsdisplay_param *);

struct cfattach acpivout_ca = {
	sizeof(struct acpivout_softc), acpivout_match, acpivout_attach
};

struct cfdriver acpivout_cd = {
	NULL, "acpivout", DV_DULL
};

int
acpivout_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aaa = aux;
	struct cfdata		*cf = match;

	if (aaa->aaa_name == NULL ||
	    strcmp(aaa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aaa->aaa_table != NULL)
		return (0);

	return (1);
}

void
acpivout_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpivout_softc	*sc = (struct acpivout_softc *)self;
	struct acpi_attach_args	*aaa = aux;

	sc->sc_acpi = ((struct acpivideo_softc *)parent)->sc_acpi;
	sc->sc_devnode = aaa->aaa_node;

	printf(": %s\n", sc->sc_devnode->name);

	aml_register_notify(sc->sc_devnode, aaa->aaa_dev,
	    acpivout_notify, sc, ACPIDEV_NOPOLL);

	ws_get_param = acpivout_get_param;
	ws_set_param = acpivout_set_param;

	acpivout_get_bcl(sc);
}

int
acpivout_notify(struct aml_node *node, int notify, void *arg)
{
	struct acpivout_softc *sc = arg;

	switch (notify) {
	case NOTIFY_BRIGHTNESS_CYCLE:
		acpivout_brightness_cycle(sc);
		break;
	case NOTIFY_BRIGHTNESS_UP:
		acpivout_brightness_up(sc);
		break;
	case NOTIFY_BRIGHTNESS_DOWN:
		acpivout_brightness_down(sc);
		break;
	case NOTIFY_BRIGHTNESS_ZERO:
		acpivout_brightness_zero(sc);
		break;
	case NOTIFY_DISPLAY_OFF:
		/* TODO: D3 state change */
		break;
	default:
		printf("%s: unknown event 0x%02x\n", DEVNAME(sc), notify);
		break;
	}

	return (0);
}

void
acpivout_brightness_cycle(struct acpivout_softc *sc)
{
	int	cur_level;

	if (sc->sc_bcl_len == 0)
		return;
	cur_level = acpivout_get_brightness(sc);
	if (cur_level == sc->sc_bcl[sc->sc_bcl_len - 1])
		acpivout_brightness_zero(sc);
	else
		acpivout_brightness_up(sc);
}

void
acpivout_brightness_up(struct acpivout_softc *sc)
{
	int i, cur_level;

	if (sc->sc_bcl_len == 0)
		return;
	cur_level = acpivout_get_brightness(sc);
	if (cur_level == -1)
		return;

	/* check for max brightness level */
	if (cur_level == sc->sc_bcl[sc->sc_bcl_len - 1])
		return;

	for (i = 0; i < sc->sc_bcl_len && cur_level != sc->sc_bcl[i]; i++);
	acpivout_set_brightness(sc, sc->sc_bcl[i + 1]);
}

void
acpivout_brightness_down(struct acpivout_softc *sc)
{
	int i, cur_level;

	if (sc->sc_bcl_len == 0)
		return;
	cur_level = acpivout_get_brightness(sc);
	if (cur_level == -1)
		return;

	/* check for min brightness level */
	if (cur_level == sc->sc_bcl[0])
		return;

	for (i = 0; i < sc->sc_bcl_len && cur_level != sc->sc_bcl[i]; i++);
	acpivout_set_brightness(sc, sc->sc_bcl[i - 1]);
}

void
acpivout_brightness_zero(struct acpivout_softc *sc)
{
	if (sc->sc_bcl_len == 0)
		return;
	acpivout_set_brightness(sc, sc->sc_bcl[0]);
}

int
acpivout_get_brightness(struct acpivout_softc *sc)
{
	struct aml_value res;
	int level;

	aml_evalname(sc->sc_acpi, sc->sc_devnode, "_BQC", 0, NULL, &res);
	level = aml_val2int(&res);
	aml_freevalue(&res);
	DPRINTF(("%s: BQC = %d\n", DEVNAME(sc), level));

	if (level < sc->sc_bcl[0] || level > sc->sc_bcl[sc->sc_bcl_len -1])
		level = -1;

	return (level);
}

int
acpivout_find_brightness(struct acpivout_softc *sc, int level)
{
	int i, mid;

	for (i = 0; i < sc->sc_bcl_len - 1; i++) {
		mid = sc->sc_bcl[i] + (sc->sc_bcl[i + 1] - sc->sc_bcl[i]) / 2;
		if (sc->sc_bcl[i] <= level && level <=  mid)
			return sc->sc_bcl[i];
		if  (mid < level && level <= sc->sc_bcl[i + 1])
			return sc->sc_bcl[i + 1];
	}
	if (level < sc->sc_bcl[0])
		return sc->sc_bcl[0];
	else
		return sc->sc_bcl[i];
}

void
acpivout_set_brightness(struct acpivout_softc *sc, int level)
{
	struct aml_value args, res;

	memset(&args, 0, sizeof(args));
	args.v_integer = level;
	args.type = AML_OBJTYPE_INTEGER;

	DPRINTF(("%s: BCM = %d\n", DEVNAME(sc), level));
	aml_evalname(sc->sc_acpi, sc->sc_devnode, "_BCM", 1, &args, &res);

	aml_freevalue(&res);
}

void
acpivout_get_bcl(struct acpivout_softc *sc)
{
	int	i, j, value;
	struct aml_value res;

	DPRINTF(("Getting _BCL!"));
	aml_evalname(sc->sc_acpi, sc->sc_devnode, "_BCL", 0, NULL, &res);
	if (res.type != AML_OBJTYPE_PACKAGE) {
		sc->sc_bcl_len = 0;
		goto err;
	}
	/*
	 * Per the ACPI spec section B.6.2 the _BCL method returns a package.
	 * The first integer in the package is the brightness level
	 * when the computer has full power, and the second is the
	 * brightness level when the computer is on batteries.
	 * All other levels may be used by OSPM.
	 * So we skip the first two integers in the package.
	 */
	if (res.length <= 2) {
		sc->sc_bcl_len = 0;
		goto err;
	}
	sc->sc_bcl_len = res.length - 2;

	sc->sc_bcl = malloc(sc->sc_bcl_len * sizeof(int), M_DEVBUF,
	    M_WAITOK | M_ZERO);
	if (sc->sc_bcl == NULL) {
		sc->sc_bcl_len = 0;
		goto err;
	}

	for (i = 0; i < sc->sc_bcl_len; i++) {
		/* Sort darkest to brightest */
		value = aml_val2int(res.v_package[i + 2]);
		for (j = i; j > 0 && sc->sc_bcl[j - 1] > value; j--)
			sc->sc_bcl[j] = sc->sc_bcl[j - 1];
		 sc->sc_bcl[j] = value;
	}

err:
	aml_freevalue(&res);
}


int
acpivout_get_param(struct wsdisplay_param *dp)
{
	struct acpivout_softc	*sc = NULL;
	int i;

	switch (dp->param) {
	case WSDISPLAYIO_PARAM_BRIGHTNESS:
		for (i = 0; i < acpivout_cd.cd_ndevs; i++) {
			if (acpivout_cd.cd_devs[i] == NULL)
				continue;
			sc = (struct acpivout_softc *)acpivout_cd.cd_devs[i];
			/* Ignore device if not connected. */
			if (sc->sc_bcl_len != 0)
				break;
		}
		if (sc != NULL && sc->sc_bcl_len != 0) {
			dp->min = 0;
			dp->max =  sc->sc_bcl[sc->sc_bcl_len - 1];
			rw_enter_write(&sc->sc_acpi->sc_lck);
			dp->curval = acpivout_get_brightness(sc);
			rw_exit_write(&sc->sc_acpi->sc_lck);
			if (dp->curval != -1)
				return 0;
		}
		return -1;
	default:
		return -1;
	}
}

int
acpivout_set_param(struct wsdisplay_param *dp)
{
	struct acpivout_softc	*sc = NULL;
	int i, exact;

	switch (dp->param) {
	case WSDISPLAYIO_PARAM_BRIGHTNESS:
		for (i = 0; i < acpivout_cd.cd_ndevs; i++) {
			if (acpivout_cd.cd_devs[i] == NULL)
				continue;
			sc = (struct acpivout_softc *)acpivout_cd.cd_devs[i];
			/* Ignore device if not connected. */
			if (sc->sc_bcl_len != 0)
				break;
		}
		if (sc != NULL && sc->sc_bcl_len != 0) {
			rw_enter_write(&sc->sc_acpi->sc_lck);
			exact = acpivout_find_brightness(sc, dp->curval);
			acpivout_set_brightness(sc, exact);
			rw_exit_write(&sc->sc_acpi->sc_lck);
			return 0;
		}
		return -1;
	default:
		return -1;
	}
}
