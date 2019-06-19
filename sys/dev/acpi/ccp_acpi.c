/*	$OpenBSD: ccp_acpi.c,v 1.1 2019/04/23 18:34:06 kettenis Exp $	*/
/*
 * Copyright (c) 2019 Mark Kettenis
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

#include <dev/ic/ccpvar.h>

struct ccp_acpi_softc {
	struct ccp_softc sc;
	struct acpi_softc *sc_acpi;
	struct aml_node *sc_node;

	bus_addr_t sc_addr;
	bus_size_t sc_size;
};

int	ccp_acpi_match(struct device *, void *, void *);
void	ccp_acpi_attach(struct device *, struct device *, void *);

struct cfattach ccp_acpi_ca = {
	sizeof(struct ccp_acpi_softc), ccp_acpi_match, ccp_acpi_attach
};

const char *ccp_hids[] = {
	"AMDI0C00",
	NULL
};

int	ccp_acpi_parse_resources(int, union acpi_resource *, void *);

int
ccp_acpi_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, ccp_hids, cf->cf_driver->cd_name);
}

void
ccp_acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct ccp_acpi_softc *sc = (struct ccp_acpi_softc *)self;
	struct aml_value res;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;
	printf(" %s", sc->sc_node->name);

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_CRS", 0, NULL, &res)) {
		printf(": can't find registers\n");
		return;
	}

	aml_parse_resource(&res, ccp_acpi_parse_resources, sc);
	printf(" addr 0x%lx/0x%lx", sc->sc_addr, sc->sc_size);
	if (sc->sc_addr == 0 || sc->sc_size == 0) {
		printf("\n");
		return;
	}

	sc->sc.sc_iot = aaa->aaa_memt;
	if (bus_space_map(sc->sc.sc_iot, sc->sc_addr, sc->sc_size, 0,
	    &sc->sc.sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	ccp_attach(&sc->sc);
}

int
ccp_acpi_parse_resources(int crsidx, union acpi_resource *crs, void *arg)
{
	struct ccp_acpi_softc *sc = arg;
	int type = AML_CRSTYPE(crs);

	switch (type) {
	case LR_MEM32FIXED:
		sc->sc_addr = crs->lr_m32fixed._bas;
		sc->sc_size = crs->lr_m32fixed._len;
		break;
	}

	return 0;
}
