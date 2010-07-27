/*	$OpenBSD: acpivideo.c,v 1.7 2010/07/27 06:12:50 deraadt Exp $	*/
/*
 * Copyright (c) 2008 Federico G. Schwindt <fgsch@openbsd.org>
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

int	acpivideo_match(struct device *, void *, void *);
void	acpivideo_attach(struct device *, struct device *, void *);
int	acpivideo_notify(struct aml_node *, int, void *);

void	acpivideo_set_policy(struct acpivideo_softc *, int);
void	acpivideo_get_dod(struct acpivideo_softc *);
int	acpi_foundvout(struct aml_node *, void *);
int	acpivideo_print(void *, const char *);

int	acpivideo_getpcibus(struct acpivideo_softc *, struct aml_node *);

struct cfattach acpivideo_ca = {
	sizeof(struct acpivideo_softc), acpivideo_match, acpivideo_attach
};

struct cfdriver acpivideo_cd = {
	NULL, "acpivideo", DV_DULL
};

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
	struct acpi_attach_args *aaa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aaa->aaa_node;

	printf(": %s\n", sc->sc_devnode->name);

	if (acpivideo_getpcibus(sc, sc->sc_devnode) == -1)
		return;

	aml_register_notify(sc->sc_devnode, aaa->aaa_dev,
	    acpivideo_notify, sc, ACPIDEV_NOPOLL);

	acpivideo_set_policy(sc,
	    DOS_SWITCH_BY_OSPM | DOS_BRIGHTNESS_BY_OSPM);

	acpivideo_get_dod(sc);
	aml_find_node(aaa->aaa_node, "_DCS", acpi_foundvout, sc);
	aml_find_node(aaa->aaa_node, "_BCL", acpi_foundvout, sc);
}

int
acpivideo_notify(struct aml_node *node, int notify, void *arg)
{
	struct acpivideo_softc *sc = arg;

	switch (notify) {
	case NOTIFY_OUTPUT_SWITCHED:
	case NOTIFY_OUTPUT_CHANGED:
	case NOTIFY_OUTPUT_CYCLE_KEY:
	case NOTIFY_OUTPUT_NEXT_KEY:
	case NOTIFY_OUTPUT_PREV_KEY:
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
	DPRINTF(("%s: set policy to %d", DEVNAME(sc), aml_val2int(&res)));

	aml_freevalue(&res);
}

int
acpi_foundvout(struct aml_node *node, void *arg)
{
	struct aml_value	res;
	int	i, addr;
	char	fattach = 0;

	struct acpivideo_softc *sc = (struct acpivideo_softc *)arg;
	struct device *self = (struct device *)arg;
	struct acpivideo_attach_args av;

	if (sc->sc_dod == NULL)
		return (0);
	DPRINTF(("Inside acpi_foundvout()"));
	if (aml_evalname(sc->sc_acpi, node->parent, "_ADR", 0, NULL, &res)) {
		DPRINTF(("%s: no _ADR\n", DEVNAME(sc)));
		return (0);
	}
	addr = aml_val2int(&res);
	DPRINTF(("_ADR: %X\n", addr));
	aml_freevalue(&res);

	for (i = 0; i < sc->sc_dod_len; i++)
		if (addr == (sc->sc_dod[i]&0xffff)) {
			DPRINTF(("Matched: %X\n", sc->sc_dod[i]));
			fattach = 1;
			break;
		}
	if (fattach) {
		memset(&av, 0, sizeof(av));
		av.aaa.aaa_iot = sc->sc_acpi->sc_iot;
		av.aaa.aaa_memt = sc->sc_acpi->sc_memt;
		av.aaa.aaa_node = node->parent;
		av.aaa.aaa_name = "acpivout";
		av.dod = sc->sc_dod[i];
		/*
		 *  Make sure we don't attach twice if both _BCL and
		 * _DCS methods are found by zeroing the DOD address.
		 */
		sc->sc_dod[i] = 0;

		config_found(self, &av, acpivideo_print);
	}

	return (0);
}

int
acpivideo_print(void *aux, const char *pnp)
{
	struct acpi_attach_args *aa = aux;

	if (pnp) {
		if (aa->aaa_name)
			printf("%s at %s", aa->aaa_name, pnp);
		else
			return (QUIET);
	}

	return (UNCONF);
}

void
acpivideo_get_dod(struct acpivideo_softc * sc)
{
	struct aml_value	res;
	int	i;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_DOD", 0, NULL, &res)) {
		DPRINTF(("%s: no _DOD\n", DEVNAME(sc)));
		return;
	}
	sc->sc_dod_len = res.length;
	if (sc->sc_dod_len == 0) {
		sc->sc_dod = NULL;
		aml_freevalue(&res);
		return;
	}
	sc->sc_dod = malloc(sc->sc_dod_len * sizeof(int), M_DEVBUF,
	    M_WAITOK|M_ZERO);
	if (sc->sc_dod == NULL) {
		aml_freevalue(&res);
		return;
	}

	for (i = 0; i < sc->sc_dod_len; i++) {
		sc->sc_dod[i] = aml_val2int(res.v_package[i]);
		DPRINTF(("DOD: %X ", sc->sc_dod[i]));
	}
	DPRINTF(("\n"));

	aml_freevalue(&res);
}

int
acpivideo_getpcibus(struct acpivideo_softc *sc, struct aml_node *node)
{
	/* Check if parent device has PCI mapping */
	return (node->parent && node->parent->pci) ?
		node->parent->pci->sub : -1;
}
