/* $OpenBSD: acpibtn.c,v 1.6 2006/02/21 17:14:42 jordan Exp $ */
/*
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
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
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <sys/sensors.h>

int acpibtn_match(struct device *, void *, void *);
void acpibtn_attach(struct device *, struct device *, void *);
int  acpibtn_notify(struct aml_node *, int, void *);

struct acpibtn_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	int			sc_btn_type;
#define ACPIBTN_LID	0
#define ACPIBTN_POWER	1
#define ACPIBTN_SLEEP	2
};

int acpibtn_getsta(struct acpibtn_softc *);

struct cfattach acpibtn_ca = {
	sizeof(struct acpibtn_softc), acpibtn_match, acpibtn_attach
};

struct cfdriver acpibtn_cd = {
	NULL, "acpibtn", DV_DULL
};

int
acpibtn_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata		*cf = match;

	/* sanity */
	if (aa->aaa_name == NULL ||
	    strcmp(aa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aa->aaa_table != NULL)
		return (0);

	return (1);
}

void
acpibtn_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpibtn_softc	*sc = (struct acpibtn_softc *)self;
	struct acpi_attach_args *aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node->child;

	if (!strcmp(aa->aaa_dev, ACPI_DEV_LD))
		sc->sc_btn_type = ACPIBTN_LID;
	if (!strcmp(aa->aaa_dev, ACPI_DEV_PBD)) {
		sc->sc_btn_type = ACPIBTN_POWER;
		sc->sc_acpi->sc_pbtndev = sc->sc_devnode;
	}
	if (!strcmp(aa->aaa_dev, ACPI_DEV_SBD)) {
		sc->sc_btn_type = ACPIBTN_SLEEP;
		sc->sc_acpi->sc_sbtndev = sc->sc_devnode;
	}

	acpibtn_getsta(sc); 

	printf(": %s\n", sc->sc_devnode->parent->name);

	aml_register_notify(sc->sc_devnode->parent, acpibtn_notify, sc);
}

int
acpibtn_getsta(struct acpibtn_softc *sc)
{
	struct aml_value	res, env;
	struct acpi_context	*ctx;

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	ctx = NULL;
	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_STA", &res, &env))
		dnprintf(20, "%s: no _STA\n", DEVNAME(sc));
		/* XXX not all buttons have _STA so FALLTROUGH */

	return (0);
}

int
acpibtn_notify(struct aml_node *node, int notify_type, void *arg)
{
	struct acpibtn_softc	*sc = arg;
	extern int		acpi_s5;

	dnprintf(10, "acpibtn_notify: %.2x %s\n", notify_type,
	    sc->sc_devnode->parent->name);

	switch (sc->sc_btn_type) {
	case ACPIBTN_LID:
	case ACPIBTN_SLEEP:
		break;
	case ACPIBTN_POWER:
		acpi_s5 = 1;
		psignal(initproc, SIGUSR1);
		/* NOTREACHED */
		break;
	default:
		printf("%s: spurious acpi interrupt\n", DEVNAME(sc));
		break;
	}

	return (0);
}
