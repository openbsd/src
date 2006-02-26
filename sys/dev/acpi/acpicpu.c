/* $OpenBSD: acpicpu.c,v 1.1 2006/02/26 02:49:28 marco Exp $ */
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

int	acpicpu_match(struct device *, void *, void *);
void	acpicpu_attach(struct device *, struct device *, void *);
int	acpicpu_notify(struct aml_node *, int, void *);

struct acpicpu_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;
};

int	acpicpu_getsta(struct acpicpu_softc *);

struct cfattach acpicpu_ca = {
	sizeof(struct acpicpu_softc), acpicpu_match, acpicpu_attach
};

struct cfdriver acpicpu_cd = {
	NULL, "acpicpu", DV_DULL
};

int
acpicpu_match(struct device *parent, void *match, void *aux)
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
acpicpu_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpicpu_softc	*sc = (struct acpicpu_softc *)self;
	struct acpi_attach_args *aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node->child;

	acpicpu_getsta(sc); 

	printf(":\n");

	aml_register_notify(sc->sc_devnode->parent, aa->aaa_dev, acpicpu_notify, sc);
}

int
acpicpu_getsta(struct acpicpu_softc *sc)
{
	struct aml_value	res, env;
	struct acpi_context	*ctx;

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	ctx = NULL;
	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_STA", &res, &env)) {
		dnprintf(20, "%s: no _STA\n", DEVNAME(sc));
		/* XXX not all buttons have _STA so FALLTROUGH */
	}

	return (0);
}

int
acpicpu_notify(struct aml_node *node, int notify_type, void *arg)
{
	struct acpicpu_softc	*sc = arg;

	dnprintf(10, "acpicpu_notify: %.2x %s\n", notify_type,
	    sc->sc_devnode->parent->name);

	printf("acpicpu_notify: %.2x %s\n", notify_type,
	    sc->sc_devnode->parent->name);

	return (0);
}
