/* $OpenBSD: ipmi_acpi.c,v 1.1 2018/03/14 18:52:16 patrick Exp $ */
/*
 * Copyright (c) 2018 Patrick Wildt <patrick@blueri.se>
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
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/apmvar.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>
#undef DEVNAME

#include <dev/ipmivar.h>

#define DEVNAME(s)		((s)->sc.sc_dev.dv_xname)

int	ipmi_acpi_match(struct device *, void *, void *);
void	ipmi_acpi_attach(struct device *, struct device *, void *);
int	ipmi_acpi_parse_crs(int, union acpi_resource *, void *);

extern void ipmi_attach(struct device *, struct device *, void *);

struct ipmi_acpi_softc {
	struct ipmi_softc	 sc;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	int			 sc_ift;

	bus_space_tag_t		 sc_iot;
	bus_size_t		 sc_iobase;
	int			 sc_iospacing;
	char			 sc_iotype;
};

struct cfattach ipmi_acpi_ca = {
	sizeof(struct ipmi_acpi_softc), ipmi_acpi_match, ipmi_acpi_attach,
};

const char *ipmi_acpi_hids[] = { ACPI_DEV_IPMI, NULL };

int
ipmi_acpi_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aa = aux;
	struct cfdata *cf = match;

	/* sanity */
	return (acpi_matchhids(aa, ipmi_acpi_hids, cf->cf_driver->cd_name));
}

void
ipmi_acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct ipmi_acpi_softc *sc = (struct ipmi_acpi_softc *)self;
	struct acpi_attach_args *aa = aux;
	struct ipmi_attach_args ia;
	struct aml_value res;
	int64_t ift;
	int rc;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	rc = aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "_IFT", 0, NULL, &ift);
	if (rc) {
		printf(": no _IFT\n");
		return;
	}
	sc->sc_ift = ift;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_CRS", 0, NULL, &res)) {
		printf(": no _CRS method\n");
		return;
	}
	if (res.type != AML_OBJTYPE_BUFFER) {
		printf(": invalid _CRS object (type %d len %d)\n",
		    res.type, res.length);
		aml_freevalue(&res);
		return;
	}

	aml_parse_resource(&res, ipmi_acpi_parse_crs, sc);
	aml_freevalue(&res);

	if (sc->sc_iot == NULL) {
		printf("%s: incomplete resources (ift %d)\n",
		    DEVNAME(sc), sc->sc_ift);
		return;
	}

	ia.iaa_iot = sc->sc_iot;
	ia.iaa_memt = sc->sc_iot;
	ia.iaa_if_type = sc->sc_ift;
	ia.iaa_if_rev = 0;
	ia.iaa_if_irq = -1;
	ia.iaa_if_irqlvl = 0;
	ia.iaa_if_iospacing = sc->sc_iospacing;
	ia.iaa_if_iobase = sc->sc_iobase;
	ia.iaa_if_iotype = sc->sc_iotype;

	ipmi_attach(parent, self, &ia);
}

int
ipmi_acpi_parse_crs(int crsidx, union acpi_resource *crs, void *arg)
{
	struct ipmi_acpi_softc *sc = arg;
	int type = AML_CRSTYPE(crs);

	switch (crsidx) {
	case 0:
		if (type != SR_IOPORT) {
			printf("%s: Unexpected resource #%d type %d\n",
			    DEVNAME(sc), crsidx, type);
			break;
		}
		sc->sc_iot = sc->sc_acpi->sc_iot;
		sc->sc_iobase = crs->sr_ioport._max;
		sc->sc_iospacing = 1;
		sc->sc_iotype = 'i';
		break;
	case 1:
		if (type != SR_IOPORT) {
			printf("%s: Unexpected resource #%d type %d\n",
			    DEVNAME(sc), crsidx, type);
			break;
		}
		if (crs->sr_ioport._max <= sc->sc_iobase)
			break;
		sc->sc_iospacing = crs->sr_ioport._max - sc->sc_iobase;
		break;
	default:
		printf("%s: invalid resource #%d type %d (ift %d)\n",
		    DEVNAME(sc), crsidx, type, sc->sc_ift);
	}

	return 0;
}
