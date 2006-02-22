/* $OpenBSD: acpibat.c,v 1.18 2006/02/22 15:29:23 marco Exp $ */
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/lock.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <sys/sensors.h>

int	acpibat_match(struct device *, void *, void *);
void	acpibat_attach(struct device *, struct device *, void *);

struct acpibat_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	struct lock		sc_lock;
	struct acpibat_bif	sc_bif;
	struct acpibat_bst	sc_bst;
	volatile int		sc_bat_present;

	struct sensor		sc_sens[8]; /* XXX debug only */
};

struct cfattach acpibat_ca = {
	sizeof(struct acpibat_softc), acpibat_match, acpibat_attach
};

struct cfdriver acpibat_cd = {
	NULL, "acpibat", DV_DULL
};

void	acpibat_monitor(struct acpibat_softc *);
void	acpibat_refresh(void *);
int	acpibat_getbif(struct acpibat_softc *);
int	acpibat_getbst(struct acpibat_softc *);
int	acpibat_notify(struct aml_node *, int, void *);

int
acpibat_match(struct device *parent, void *match, void *aux)
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
acpibat_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpibat_softc	*sc = (struct acpibat_softc *)self;
	struct acpi_attach_args	*aa = aux;
	struct aml_value	res, env;
	struct acpi_context	*ctx;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node->child;

	lockinit(&sc->sc_lock, PZERO, DEVNAME(sc), 0, 0);

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	/* XXX this trick seems to only work during boot */
	ctx = NULL;
	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_STA", &res, &env))
		dnprintf(10, "%s: no _STA\n",
		    DEVNAME(sc));

	if (!(res.v_integer & STA_BATTERY)) {
		sc->sc_bat_present = 0;
		printf(": %s: not present\n", sc->sc_devnode->parent->name);
		acpibat_monitor(sc);
	} else {
		sc->sc_bat_present = 1;

		acpibat_getbif(sc);
		acpibat_getbst(sc);

		printf(": %s: model: %s serial: %s type: %s oem: %s\n",
		    sc->sc_devnode->parent->name,
		    sc->sc_bif.bif_model,
		    sc->sc_bif.bif_serial,
		    sc->sc_bif.bif_type,
		    sc->sc_bif.bif_oem);

		if (sensor_task_register(sc, acpibat_refresh, 10))
			printf(", unable to register update task\n");

		acpibat_monitor(sc);

	}

	aml_register_notify(sc->sc_devnode->parent, acpibat_notify, sc);
}

/* XXX this is for debug only, remove later */
void
acpibat_monitor(struct acpibat_softc *sc)
{
	int			i;

	/* assume _BIF and _BST have been called */

	memset(sc->sc_sens, 0, sizeof(sc->sc_sens));
	for (i = 0; i < 8; i++)
		strlcpy(sc->sc_sens[i].device, DEVNAME(sc),
		    sizeof(sc->sc_sens[i].device));

	/* XXX ugh but make sure */
	if (!sc->sc_bif.bif_cap_granu1)
		sc->sc_bif.bif_cap_granu1 = 1;

	strlcpy(sc->sc_sens[0].desc, "last full capacity",
	    sizeof(sc->sc_sens[2].desc));
	sc->sc_sens[0].type = SENSOR_PERCENT;
	sensor_add(&sc->sc_sens[0]);
	sc->sc_sens[0].value = sc->sc_bif.bif_last_capacity /
	    sc->sc_bif.bif_cap_granu1 * 1000;

	strlcpy(sc->sc_sens[1].desc, "warning capacity",
	    sizeof(sc->sc_sens[1].desc));
	sc->sc_sens[1].type = SENSOR_PERCENT;
	sensor_add(&sc->sc_sens[1]);
	sc->sc_sens[1].value = sc->sc_bif.bif_warning /
	    sc->sc_bif.bif_cap_granu1 * 1000;

	strlcpy(sc->sc_sens[2].desc, "low capacity",
	    sizeof(sc->sc_sens[2].desc));
	sc->sc_sens[2].type = SENSOR_PERCENT;
	sensor_add(&sc->sc_sens[2]);
	sc->sc_sens[2].value = sc->sc_bif.bif_warning /
	    sc->sc_bif.bif_cap_granu1 * 1000;

	strlcpy(sc->sc_sens[3].desc, "voltage", sizeof(sc->sc_sens[3].desc));
	sc->sc_sens[3].type = SENSOR_VOLTS_DC;
	sensor_add(&sc->sc_sens[3]);
	sc->sc_sens[3].status = SENSOR_S_OK;
	sc->sc_sens[3].value = sc->sc_bif.bif_voltage * 1000;

	strlcpy(sc->sc_sens[4].desc, "state", sizeof(sc->sc_sens[4].desc));
	sc->sc_sens[4].type = SENSOR_INTEGER;
	sensor_add(&sc->sc_sens[4]);
	sc->sc_sens[4].status = SENSOR_S_OK;
	sc->sc_sens[4].value = sc->sc_bst.bst_state;

	strlcpy(sc->sc_sens[5].desc, "rate", sizeof(sc->sc_sens[5].desc));
	sc->sc_sens[5].type = SENSOR_INTEGER;
	sensor_add(&sc->sc_sens[5]);
	sc->sc_sens[5].value = sc->sc_bst.bst_rate;

	strlcpy(sc->sc_sens[6].desc, "remaining capacity",
	    sizeof(sc->sc_sens[6].desc));
	sc->sc_sens[6].type = SENSOR_PERCENT;
	sensor_add(&sc->sc_sens[6]);
	sc->sc_sens[6].value = sc->sc_bst.bst_capacity /
	    sc->sc_bif.bif_cap_granu1 * 1000;

	strlcpy(sc->sc_sens[7].desc, "current voltage",
	    sizeof(sc->sc_sens[7].desc));
	sc->sc_sens[7].type = SENSOR_VOLTS_DC;
	sensor_add(&sc->sc_sens[7]);
	sc->sc_sens[7].status = SENSOR_S_OK;
	sc->sc_sens[7].value = sc->sc_bst.bst_voltage * 1000;
}

void
acpibat_refresh(void *arg)
{
	struct acpibat_softc	*sc = arg;

	dnprintf(30, "%s: %s: refresh\n", DEVNAME(sc),
	    sc->sc_devnode->parent->name);

	acpibat_getbif(sc);
	acpibat_getbst(sc); 

	lockmgr(&sc->sc_lock, LK_EXCLUSIVE, NULL);

	/* XXX ugh but make sure */
	if (!sc->sc_bif.bif_cap_granu1)
		sc->sc_bif.bif_cap_granu1 = 1;

	sc->sc_sens[0].value = sc->sc_bif.bif_last_capacity /
	    sc->sc_bif.bif_cap_granu1 * 1000;
	sc->sc_sens[1].value = sc->sc_bif.bif_warning /
	    sc->sc_bif.bif_cap_granu1 * 1000;
	sc->sc_sens[2].value = sc->sc_bif.bif_warning /
	    sc->sc_bif.bif_cap_granu1 * 1000;
	sc->sc_sens[3].value = sc->sc_bif.bif_voltage * 1000;

	sc->sc_sens[4].status = SENSOR_S_OK;
	if (sc->sc_bst.bst_state & BST_DISCHARGE)
		strlcpy(sc->sc_sens[4].desc, "battery discharging",
		    sizeof(sc->sc_sens[4].desc));
	else if (sc->sc_bst.bst_state & BST_CHARGE)
		strlcpy(sc->sc_sens[4].desc, "battery charging",
		    sizeof(sc->sc_sens[4].desc));
	else if (sc->sc_bst.bst_state & BST_CRITICAL) {
		strlcpy(sc->sc_sens[4].desc, "battery critical",
		    sizeof(sc->sc_sens[4].desc));
		sc->sc_sens[4].status = SENSOR_S_CRIT;
	}
	sc->sc_sens[4].value = sc->sc_bst.bst_state;
	sc->sc_sens[5].value = sc->sc_bst.bst_rate;
	sc->sc_sens[6].value = sc->sc_bst.bst_capacity /
	    sc->sc_bif.bif_cap_granu1 * 1000;
	sc->sc_sens[7].value = sc->sc_bst.bst_voltage * 1000;

	lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
}

int
acpibat_getbif(struct acpibat_softc *sc)
{
	struct aml_value	res, env;
	struct acpi_context	*ctx;
	int			rv = 1;

	lockmgr(&sc->sc_lock, LK_EXCLUSIVE, NULL);

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	ctx = NULL;
	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_STA", &res, &env)) {
		dnprintf(10, "%s: no _STA\n",
		    DEVNAME(sc));
		goto out;
	}

	/* XXX this is broken, it seems to only work during boot */
	/*
	if (!(res.v_integer & STA_BATTERY)) {
		sc->sc_bat_present = 0;
		return (1);
	} else
		sc->sc_bat_present = 1;
	*/

	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_BIF", &res, &env)) {
		dnprintf(10, "%s: no _BIF\n",
		    DEVNAME(sc));
		printf("bif fails\n");
		goto out;
	}

	if (res.length != 13) {
		printf("%s: invalid _BIF, battery information not saved\n",
		    DEVNAME(sc));
		goto out;
	}

	memset(&sc->sc_bif, 0, sizeof sc->sc_bif);
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

	dnprintf(60, "power_unit: %u capacity: %u last_cap: %u tech: %u "
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

out:
	lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
	return (rv);
}

int
acpibat_getbst(struct acpibat_softc *sc)
{
	struct aml_value	res, env;
	struct acpi_context	*ctx;
	int			rv;

	lockmgr(&sc->sc_lock, LK_EXCLUSIVE, NULL);

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	ctx = NULL;
	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_BST", &res, &env)) {
		dnprintf(10, "%s: no _BST\n",
		    DEVNAME(sc));
		printf("_bst fails\n");
		goto out;
	}

	if (res.length != 4) {
		printf("%s: invalid _BST, battery status not saved\n",
		    DEVNAME(sc));
		goto out;
	}

	sc->sc_bst.bst_state = aml_val2int(ctx, res.v_package[0]);
	sc->sc_bst.bst_rate = aml_val2int(ctx, res.v_package[1]);
	sc->sc_bst.bst_capacity = aml_val2int(ctx, res.v_package[2]);
	sc->sc_bst.bst_voltage = aml_val2int(ctx, res.v_package[3]);

	dnprintf(60, "state: %u rate: %u cap: %u volt: %u ",
	    sc->sc_bst.bst_state,
	    sc->sc_bst.bst_rate,
	    sc->sc_bst.bst_capacity,
	    sc->sc_bst.bst_voltage);
out:
	lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
	return (rv);
}

/* XXX it has been observed that some systemts do not propagate battery
 * inserion events up to the driver.  What seems to happen is that DSDT
 * does receive an interrupt however the originator bit is not set.
 * This seems to happen when one inserts a 100% full battery.  Removal
 * of the power cord or insertion of a not 100% full battery breaks this
 * behavior and all events will then be sent upwards.  Currently there
 * is no known work-around for it.
 */

int
acpibat_notify(struct aml_node *node, int notify_type, void *arg)
{
	struct acpibat_softc	*sc = arg;

	dnprintf(10, "acpibat_notify: %.2x %s\n", notify_type,
	    sc->sc_devnode->parent->name);

	switch (notify_type) {
	case 0x80:	/* _BST changed */
		if (sc->sc_bat_present == 0) {
			printf("%s: %s: inserted\n", DEVNAME(sc),
			    sc->sc_devnode->parent->name);

			if (sensor_task_register(sc, acpibat_refresh, 10))
				printf(", unable to register update task\n");
		}

		sc->sc_bat_present = 1;
		acpibat_getbif(sc);
		acpibat_getbst(sc);

		break;
	case 0x81:	/* _BIF changed */
		/* XXX consider this a device removal */
		sensor_task_unregister(sc);
		sc->sc_bat_present = 0;
		strlcpy(sc->sc_sens[4].desc, "battery removed",
		    sizeof(sc->sc_sens[4].desc));
		printf("%s: %s: removed\n", DEVNAME(sc),
		    sc->sc_devnode->parent->name);

		break;
	default:
		printf("%s: unhandled battery event %x\n", DEVNAME(sc),
		    notify_type);
		break;
	}

	return (0);
}
