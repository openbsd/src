/* $OpenBSD: acpitz.c,v 1.1 2006/05/19 09:24:32 canacar Exp $ */
/*
 * Copyright (c) 2006 Can Erkin Acar <canacar@openbsd.org>
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
#include <sys/rwlock.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <sys/sensors.h>

struct acpitz_softc {
	struct device		sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	struct rwlock		sc_lock;
	u_int64_t		sc_tmp;
	u_int64_t		sc_crt;
	struct sensor		sc_sens;
};

int	acpitz_match(struct device *, void *, void *);
void	acpitz_attach(struct device *, struct device *, void *);
void	acpitz_refresh(void *);
int	acpitz_gettmp(struct acpitz_softc *);
int	acpitz_notify(struct aml_node *, int, void *);

struct cfattach acpitz_ca = {
	sizeof(struct acpitz_softc), acpitz_match, acpitz_attach
};

struct cfdriver acpitz_cd = {
	NULL, "acpitz", DV_DULL
};

void	acpitz_monitor(struct acpitz_softc *);
void	acpitz_refresh(void *);
int	acpitz_getbif(struct acpitz_softc *);
int	acpitz_getbst(struct acpitz_softc *);
int	acpitz_notify(struct aml_node *, int, void *);
int	acpitz_getcrt(struct acpitz_softc *);

int
acpitz_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata		*cf = match;

	/* sanity */
	if (aa->aaa_name == NULL ||
	    strcmp(aa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aa->aaa_table != NULL)
		return (0);

	if (aa->aaa_node->opcode != AMLOP_THERMALZONE)
		return (0);

	return (1);
}

void
acpitz_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpitz_softc	*sc = (struct acpitz_softc *)self;
	struct acpi_attach_args	*aa = aux;
	struct aml_value	res, env;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node->child;

	rw_init(&sc->sc_lock, "acpitz");

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	if (acpitz_gettmp(sc)) {
		printf(", failed to read _TMP");
		return;
	}

	if (acpitz_getcrt(sc)) {
		printf(", no critical temperature defined!");
		sc->sc_crt = 0;
	} else
		printf(", critical temperature: %u degC",
		    (unsigned)(sc->sc_crt - 2732) / 10);

	aml_register_notify(sc->sc_devnode->parent, aa->aaa_dev,
	    acpitz_notify, sc);
	
	memset(&sc->sc_sens, 0, sizeof(sc->sc_sens));
	strlcpy(sc->sc_sens.device, DEVNAME(sc), sizeof(sc->sc_sens.device));
	strlcpy(sc->sc_sens.desc, "zone temperature",
		sizeof(sc->sc_sens.desc));
	sc->sc_sens.type = SENSOR_TEMP;
	sensor_add(&sc->sc_sens);
	sc->sc_sens.value = 0;
	
	if (sensor_task_register(sc, acpitz_refresh, 10))
		printf(", unable to register update task");
	
	printf("\n");
}

void
acpitz_refresh(void *arg)
{
	struct acpitz_softc	*sc = arg;
	extern int		acpi_s5;

	dnprintf(30, "%s: %s: refresh\n", DEVNAME(sc),
	    sc->sc_devnode->parent->name);

	if (acpitz_gettmp(sc)) {
		dnprintf(30, "%s: %s: failed to read temp!\n", DEVNAME(sc),
		    sc->sc_devnode->parent->name);
		sc->sc_tmp = 0;	/* XXX */
	}

	if (sc->sc_crt && sc->sc_crt <= sc->sc_tmp) {
		/* Do critical shutdown */
		printf("%s: Critical temperature, shutting down!\n",
		    DEVNAME(sc));
		acpi_s5 = 1;
		psignal(initproc, SIGUSR1);
		/* NOTREACHED */
	}

	rw_enter_write(&sc->sc_lock);

	sc->sc_sens.value = sc->sc_tmp * 100000;

	rw_exit_write(&sc->sc_lock);
}

int
acpitz_gettmp(struct acpitz_softc *sc)
{
	struct aml_value	res, env;
	int   rv = 1;

	rw_enter_write(&sc->sc_lock);

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_TMP", &res, &env)) {
		dnprintf(10, "%s: no _TMP\n", DEVNAME(sc));
		goto out;
	}

	sc->sc_tmp = aml_val2int(NULL, &res);
	rv = 0;
 out:
	rw_exit_write(&sc->sc_lock);
	return (rv);
}

int
acpitz_getcrt(struct acpitz_softc *sc)
{
	struct aml_value	res, env;
	int   rv = 1;

	rw_enter_write(&sc->sc_lock);

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_CRT", &res, &env)) {
		dnprintf(10, "%s: no _CRT\n", DEVNAME(sc));
		goto out;
	}

	sc->sc_crt = aml_val2int(NULL, &res);
	rv = 0;
 out:
	rw_exit_write(&sc->sc_lock);
	return (rv);
}


int
acpitz_notify(struct aml_node *node, int notify_type, void *arg)
{
	struct acpitz_softc	*sc = arg;
	u_int64_t crt;

	dnprintf(10, "%s notify: %.2x %s\n", DEVNAME(sc), notify_type,
	    sc->sc_devnode->parent->name);

	switch (notify_type) {
	case 0x81:	/* Operating Points changed */
		crt = sc->sc_crt;
		acpitz_getcrt(sc);
		if (crt != sc->sc_crt)
			printf("%s: critical temperature: %u degC",
		            DEVNAME(sc),
			    (unsigned)(sc->sc_crt - 2732) / 10);
		break;
	default:
		break;
	}

	acpitz_refresh(sc);

	return (0);
}
