/* $OpenBSD: acpibat.c,v 1.7 2005/12/30 05:59:40 tedu Exp $ */
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
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

int acpibat_match(struct device *, void *, void *);
void acpibat_attach(struct device *, struct device *, void *);

struct acpibat_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	struct acpibat_bif	sc_bif;
	struct acpibat_bst	sc_bst;
};

struct cfattach acpibat_ca = {
	sizeof(struct acpibat_softc), acpibat_match, acpibat_attach
};

struct cfdriver acpibat_cd = {
	NULL, "acpibat", DV_DULL
};

int acpibat_getbif(struct acpibat_softc *);
int acpibat_getbst(struct acpibat_softc *);

int
acpibat_match(struct device *parent, void *match, void *aux)
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
acpibat_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpibat_softc *sc = (struct acpibat_softc *) self;
	struct acpi_attach_args *aa = aux;

	printf("\n");

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node->child;
	/* acpibat_getbif(sc); */

	printf("\n");
}

int
acpibat_getbif(struct acpibat_softc *sc)
{
	struct aml_value res, env;

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_BIF", &res, &env)) {
		dnprintf(50, "%s: no _BIF\n",
		    DEVNAME(sc));
		return (1);
	}

	if (res.length != 13) {
		printf("%s: invalid _BIF, battery information not saved\n",
		    DEVNAME(sc));
		return (1);
	}

	sc->sc_bif.bif_power_unit = aml_intval(&res.v_package[0]);
	sc->sc_bif.bif_capacity = aml_intval(&res.v_package[1]);
	sc->sc_bif.bif_last_capacity = aml_intval(&res.v_package[2]);
	sc->sc_bif.bif_technology = aml_intval(&res.v_package[3]);
	sc->sc_bif.bif_voltage = aml_intval(&res.v_package[4]);
	sc->sc_bif.bif_warning = aml_intval(&res.v_package[5]);
	sc->sc_bif.bif_low = aml_intval(&res.v_package[6]);
	sc->sc_bif.bif_cap_granu1 = aml_intval(&res.v_package[7]);
	sc->sc_bif.bif_cap_granu2 = aml_intval(&res.v_package[8]);
	sc->sc_bif.bif_model = aml_strval(&res.v_package[9]);
	sc->sc_bif.bif_serial = aml_strval(&res.v_package[10]);
	sc->sc_bif.bif_type = aml_strval(&res.v_package[11]);
	sc->sc_bif.bif_oem = aml_strval(&res.v_package[12]);

	dnprintf(20, "power_unit: %u capacity: %u last_cap: %u tech: %u "
	    "volt: %u warn: %u low: %u gran1: %u gran2: %d model: %s "
	    "serial: %s type: %s oem: %s\n",
	    sc->sc_bif.bif_power_unit,
	    sc->sc_bif.bif_capacity,
	    sc->sc_bif.bif_last_capacity,
	    sc->sc_bif.bif_technology,
	    sc->sc_bif.bif_voltage,
	    sc->sc_bif.bif_warning,
	    sc->sc_bif.bif_low,
	    sc->sc_bif.bif_cap_granu1,
	    sc->sc_bif.bif_cap_granu2,
	    sc->sc_bif.bif_model,
	    sc->sc_bif.bif_serial,
	    sc->sc_bif.bif_type,
	    sc->sc_bif.bif_oem);

	return (0);
}

int
acpibat_getbst(struct acpibat_softc *sc)
{
	struct aml_value res, env;

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_BST", &res, &env)) {
		dnprintf(50, "%s: no _BST\n",
		    DEVNAME(sc));
		return (1);
	}

	if (res.length != 4) {
		printf("%s: invalid _BST, battery status not saved\n",
		    DEVNAME(sc));
		return (1);
	}

	sc->sc_bst.bst_state = aml_intval(&res.v_package[0]);
	sc->sc_bst.bst_rate = aml_intval(&res.v_package[1]);
	sc->sc_bst.bst_capacity = aml_intval(&res.v_package[2]);
	sc->sc_bst.bst_voltage = aml_intval(&res.v_package[3]);

	dnprintf(20, "state: %u rate: %u cap: %u volt: %u ",
	    sc->sc_bst.bst_state,
	    sc->sc_bst.bst_rate,
	    sc->sc_bst.bst_capacity,
	    sc->sc_bst.bst_voltage);

	return (0);
}
