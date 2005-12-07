/* $OpenBSD: dsdt.c,v 1.2 2005/12/07 04:28:29 marco Exp $ */
/*
 * Copyright (c) 2005 Jordan Hargrave <jordan@openbsd.org>
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

struct dsdt_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

int	dsdtmatch(struct device *, void *, void *);
void	dsdtattach(struct device *, struct device *, void *);
int	dsdt_parse_aml(struct dsdt_softc *, u_int8_t *, u_int32_t);

struct cfattach dsdt_ca = {
	sizeof(struct dsdt_softc), dsdtmatch, dsdtattach
};

struct cfdriver dsdt_cd = {
	NULL, "dsdt", DV_DULL
};

int
dsdtmatch(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args		*aaa = aux;
	struct acpi_table_header	*hdr;

	/* if we do not have a table, it is not us */
	if (aaa->aaa_table == NULL)
		return (0);

	/* if it is an DSDT table, we can attach */
	hdr = (struct acpi_table_header *)aaa->aaa_table;
	if (memcmp(hdr->signature, DSDT_SIG, sizeof(DSDT_SIG) - 1) != 0)
		return (0);

	return (1);
}

void
dsdtattach(struct device *parent, struct device *self, void *aux)
{
	struct dsdt_softc	*sc = (struct dsdt_softc *) self;
	struct acpi_attach_args	*aa = aux;
	struct acpi_dsdt	*dsdt = (struct acpi_dsdt *)aa->aaa_table;

	dsdt_parse_aml(sc, dsdt->aml, dsdt->hdr_length - sizeof(dsdt->hdr));
}

int
dsdt_parse_aml(struct dsdt_softc *sc, u_int8_t *start, u_int32_t length)
{
	return (0);
}
