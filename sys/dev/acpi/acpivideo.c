/*	$OpenBSD: acpivideo.c,v 1.1 2008/07/02 03:14:54 fgsch Exp $	*/
/*
 * Copyright (c) 2008 Federico G. Schwindt <fgsch@openbsd.org>
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

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#ifdef ACPIVIDEO_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

/* _DOS Enable/Disable Output Switching */
#define DOS_SWITCH_BY_OSPM		0
#define DOS_SWITCH_BY_BIOS		1
#define DOS_SWITCH_LOCKED		2
#define DOS_SWITCH_BY_OSPM_EXT		3
#define DOS_BRIGHTNESS_BY_OSPM		4

/* Notifications for Displays Devices */
#define NOTIFY_OUTPUT_SWITCHED		0x80
#define NOTIFY_OUTPUT_CHANGED		0x81
#define NOTIFY_OUTPUT_CYCLE_KEY		0x82
#define NOTIFY_OUTPUT_NEXT_KEY		0x83
#define NOTIFY_OUTPUT_PREV_KEY		0x84

struct acpivideo_softc {
	struct device sc_dev;

	struct acpi_softc *sc_acpi;
	struct aml_node	*sc_devnode;
};

int	acpivideo_match(struct device *, void *, void *);
void	acpivideo_attach(struct device *, struct device *, void *);

struct cfattach acpivideo_ca = {
	sizeof(struct acpivideo_softc), acpivideo_match, acpivideo_attach
};

struct cfdriver acpivideo_cd = {
	NULL, "acpivideo", DV_DULL
};

int	acpivideo_notify(struct aml_node *, int, void *);
void	acpivideo_set_policy(struct acpivideo_softc *, int);

int
acpivideo_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aaa = aux;
	struct cfdata *cf = match;

	if (aaa->aaa_name == NULL || strcmp(aaa->aaa_name,
	    cf->cf_driver->cd_name) != 0 || aaa->aaa_table != NULL)
		return (0);

	return (1);
}

void
acpivideo_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpivideo_softc *sc = (struct acpivideo_softc *)self;
	struct acpi_attach_args *aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	printf(": %s\n", sc->sc_devnode->name);

	aml_register_notify(sc->sc_devnode, aa->aaa_dev,
	    acpivideo_notify, sc, ACPIDEV_NOPOLL);

	acpivideo_set_policy(sc,
	    DOS_SWITCH_BY_OSPM | DOS_BRIGHTNESS_BY_OSPM);
}

int
acpivideo_notify(struct aml_node *node, int notify, void *arg)
{
	struct acpivideo_softc *sc = arg;

	switch (notify) {
	case NOTIFY_OUTPUT_SWITCHED:
	case NOTIFY_OUTPUT_CHANGED:
		DPRINTF(("%s: event 0x%02x\n", DEVNAME(sc), notify));
		break;

	default:
		printf("%s: unknown event 0x%02x\n", DEVNAME(sc), notify);
		break;
	}

	return (0);
}

void
acpivideo_set_policy(struct acpivideo_softc *sc, int policy)
{
	struct aml_value args, res;

	memset(&args, 0, sizeof(args));
	args.v_integer = policy;
	args.type = AML_OBJTYPE_INTEGER;
	aml_evalname(sc->sc_acpi, sc->sc_devnode, "_DOS", 1, &args, &res);
	aml_freevalue(&res);
}
