/* $OpenBSD: acpiac.c,v 1.2 2005/12/16 04:16:59 marco Exp $ */
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>

int acpiac_match(struct device *, void *, void *);
void acpiac_attach(struct device *, struct device *, void *);

struct acpiac_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct cfattach acpiac_ca = {
	sizeof(struct acpiac_softc), acpiac_match, acpiac_attach
};

struct cfdriver acpiac_cd = {
	NULL, "acpiac", DV_DULL
};

int
acpiac_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aa = aux;
	struct cfdata *cf = match;

	/* sanity */
	if (aa->aaa_name == NULL ||
	    strcmp(aa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aa->aaa_table != NULL)
		return (0);

	return (1);
}

void
acpiac_attach(struct device *parent, struct device *self, void *aux)
{
/*
	struct acpiac_softc *sc = (struct acpiac_softc *) self;
	struct acpi_softc *psc = (struct acpi_softc *) parent;
	struct acpi_attach_args *aa = aux;
	bus_addr_t address;
	bus_size_t size;
*/

	printf("\n");
}
