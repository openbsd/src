/* $OpenBSD: acpibat.c,v 1.15 2006/02/17 07:25:51 marco Exp $ */
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

#include <sys/sensors.h>

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

	struct sensor sens[13];	/* XXX debug only */
};

struct cfattach acpibat_ca = {
	sizeof(struct acpibat_softc), acpibat_match, acpibat_attach
};

struct cfdriver acpibat_cd = {
	NULL, "acpibat", DV_DULL
};

void acpibat_refresh(void *);
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
	int i;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node->child;

	if (acpibat_getbif(sc))
		return;

	acpibat_getbst(sc); 

	printf(": model: %s serial: %s type: %s oem: %s\n",
	    sc->sc_bif.bif_model,
	    sc->sc_bif.bif_serial,
	    sc->sc_bif.bif_type,
	    sc->sc_bif.bif_oem);

	memset(sc->sens, 0, sizeof(sc->sens));

	/* XXX this is for debug only, remove later */
	for (i = 0; i < 13; i++)
		strlcpy(sc->sens[i].device, DEVNAME(sc), sizeof(sc->sens[i].device));

	strlcpy(sc->sens[0].desc, "last full capacity", sizeof(sc->sens[2].desc));
	sc->sens[0].type = SENSOR_PERCENT;
	sensor_add(&sc->sens[0]);
	sc->sens[0].value = sc->sc_bif.bif_last_capacity / sc->sc_bif.bif_cap_granu1 * 1000;

	strlcpy(sc->sens[1].desc, "warning capacity", sizeof(sc->sens[1].desc));
	sc->sens[1].type = SENSOR_PERCENT;
	sensor_add(&sc->sens[1]);
	sc->sens[1].value = sc->sc_bif.bif_warning / sc->sc_bif.bif_cap_granu1 * 1000;

	strlcpy(sc->sens[2].desc, "low capacity", sizeof(sc->sens[2].desc));
	sc->sens[2].type = SENSOR_PERCENT;
	sensor_add(&sc->sens[2]);
	sc->sens[2].value = sc->sc_bif.bif_warning / sc->sc_bif.bif_cap_granu1 * 1000;

	strlcpy(sc->sens[3].desc, "voltage", sizeof(sc->sens[3].desc));
	sc->sens[3].type = SENSOR_VOLTS_DC;
	sensor_add(&sc->sens[3]);
	sc->sens[3].status = SENSOR_S_OK;
	sc->sens[3].value = sc->sc_bif.bif_voltage * 1000;

	strlcpy(sc->sens[4].desc, "state", sizeof(sc->sens[4].desc));
	sc->sens[4].type = SENSOR_INTEGER;
	sensor_add(&sc->sens[4]);
	sc->sens[4].status = SENSOR_S_OK;
	sc->sens[4].value = sc->sc_bst.bst_state;

	strlcpy(sc->sens[5].desc, "rate", sizeof(sc->sens[5].desc));
	sc->sens[5].type = SENSOR_INTEGER;
	sensor_add(&sc->sens[5]);
	sc->sens[5].value = sc->sc_bst.bst_rate;

	strlcpy(sc->sens[6].desc, "remaining capacity", sizeof(sc->sens[6].desc));
	sc->sens[6].type = SENSOR_PERCENT;
	sensor_add(&sc->sens[6]);
	sc->sens[6].value = sc->sc_bst.bst_capacity / sc->sc_bif.bif_cap_granu1 * 1000;

	strlcpy(sc->sens[7].desc, "current voltage", sizeof(sc->sens[7].desc));
	sc->sens[7].type = SENSOR_VOLTS_DC;
	sensor_add(&sc->sens[7]);
	sc->sens[7].status = SENSOR_S_OK;
	sc->sens[7].value = sc->sc_bst.bst_voltage * 1000;

	if (sensor_task_register(sc, acpibat_refresh, 10))
		printf(", unable to register update task\n");
}

/* XXX this is for debug only, remove later */
void
acpibat_refresh(void *arg)
{
	struct acpibat_softc *sc = arg;

	acpibat_getbif(sc);
	acpibat_getbst(sc); 

	sc->sens[0].value = sc->sc_bif.bif_last_capacity / sc->sc_bif.bif_cap_granu1 * 1000;
	sc->sens[1].value = sc->sc_bif.bif_warning / sc->sc_bif.bif_cap_granu1 * 1000;
	sc->sens[2].value = sc->sc_bif.bif_warning / sc->sc_bif.bif_cap_granu1 * 1000;
	sc->sens[3].value = sc->sc_bif.bif_voltage * 1000;

	sc->sens[4].status = SENSOR_S_OK;
	if (sc->sc_bst.bst_state & BST_DISCHARGE)
		strlcpy(sc->sens[4].desc, "battery discharging", sizeof(sc->sens[4].desc));
	else if (sc->sc_bst.bst_state & BST_CHARGE)
		strlcpy(sc->sens[4].desc, "battery charging", sizeof(sc->sens[4].desc));
	else if (sc->sc_bst.bst_state & BST_CRITICAL) {
		strlcpy(sc->sens[4].desc, "battery critical", sizeof(sc->sens[4].desc));
		sc->sens[4].status = SENSOR_S_CRIT;
	}
	sc->sens[4].value = sc->sc_bst.bst_state;
	sc->sens[5].value = sc->sc_bst.bst_rate;
	sc->sens[6].value = sc->sc_bst.bst_capacity / sc->sc_bif.bif_cap_granu1 * 1000;
}

int
acpibat_getbif(struct acpibat_softc *sc)
{
	struct aml_value res, env;
	struct acpi_context *ctx;

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	ctx = NULL;
	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_STA", &res, &env)) {
		dnprintf(10, "%s: no _STA\n",
		    DEVNAME(sc));
		return (1);
	}

	if (!(res.v_integer & STA_BATTERY)) {
		printf(": battery not present\n");
		return (1);
	}

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

	sc->sc_bif.bif_power_unit = aml_val2int(ctx, res.v_package[0]);
	sc->sc_bif.bif_capacity = aml_val2int(ctx, res.v_package[1]);
	sc->sc_bif.bif_last_capacity = aml_val2int(ctx, res.v_package[2]);
	sc->sc_bif.bif_technology = aml_val2int(ctx, res.v_package[3]);
	sc->sc_bif.bif_voltage = aml_val2int(ctx, res.v_package[4]);
	sc->sc_bif.bif_warning = aml_val2int(ctx, res.v_package[5]);
	sc->sc_bif.bif_low = aml_val2int(ctx, res.v_package[6]);
	sc->sc_bif.bif_cap_granu1 = aml_val2int(ctx, res.v_package[7]);
	sc->sc_bif.bif_cap_granu2 = aml_val2int(ctx, res.v_package[8]);
	sc->sc_bif.bif_model = aml_strval(res.v_package[9]);
	sc->sc_bif.bif_serial = aml_strval(res.v_package[10]);
	sc->sc_bif.bif_type = aml_strval(res.v_package[11]);
	sc->sc_bif.bif_oem = aml_strval(res.v_package[12]);

	dnprintf(10, "power_unit: %u capacity: %u last_cap: %u tech: %u "
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
	struct acpi_context *ctx;

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	ctx = NULL;
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

	sc->sc_bst.bst_state = aml_val2int(ctx, res.v_package[0]);
	sc->sc_bst.bst_rate = aml_val2int(ctx, res.v_package[1]);
	sc->sc_bst.bst_capacity = aml_val2int(ctx, res.v_package[2]);
	sc->sc_bst.bst_voltage = aml_val2int(ctx, res.v_package[3]);

	dnprintf(10, "state: %u rate: %u cap: %u volt: %u ",
	    sc->sc_bst.bst_state,
	    sc->sc_bst.bst_rate,
	    sc->sc_bst.bst_capacity,
	    sc->sc_bst.bst_voltage);

	return (0);
}
