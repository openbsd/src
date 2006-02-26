/* $OpenBSD: acpicpu.c,v 1.3 2006/02/26 05:17:25 marco Exp $ */
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

	int			sc_pss_len;
	struct acpicpu_pss	*sc_pss;
};

int	acpicpu_getpss(struct acpicpu_softc *);

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
	int			i;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node->child;

	sc->sc_pss = NULL;

	printf(": %s: ", sc->sc_devnode->parent->name);
	if (acpicpu_getpss(sc)) {
		/* XXX not the right test but has to do for now */
		printf("can't attach, no _PSS\n");
		return;
	}

	for (i = 0; i < sc->sc_pss_len; i++)
		printf("%d MHz %d mW  ", sc->sc_pss[i].pss_core_freq,
		    sc->sc_pss[i].pss_power);
	printf("\n");

	/* aml_register_notify(sc->sc_devnode->parent, aa->aaa_dev, acpicpu_notify, sc); */
}

int
acpicpu_getpss(struct acpicpu_softc *sc)
{
	struct aml_value	res, env;
	struct acpi_context	*ctx;
	int			i;

	memset(&res, 0, sizeof(res));
	memset(&env, 0, sizeof(env));

	ctx = NULL;
	if (aml_eval_name(sc->sc_acpi, sc->sc_devnode, "_PSS", &res, &env)) {
		dnprintf(20, "%s: no _PSS\n", DEVNAME(sc));
		return (1);
	}
	
	if (!sc->sc_pss)
		sc->sc_pss = malloc(res.length * sizeof *sc->sc_pss, M_DEVBUF,
		    M_WAITOK);
	memset(sc->sc_pss, 0, res.length * sizeof *sc->sc_pss);

	for (i = 0; i < res.length; i++) {
		sc->sc_pss[i].pss_core_freq = aml_val2int(ctx,
		    res.v_package[i]->v_package[0]);
		sc->sc_pss[i].pss_power = aml_val2int(ctx,
		    res.v_package[i]->v_package[1]);
		sc->sc_pss[i].pss_trans_latency = aml_val2int(ctx,
		    res.v_package[i]->v_package[2]);
		sc->sc_pss[i].pss_bus_latency = aml_val2int(ctx,
		    res.v_package[i]->v_package[3]);
		sc->sc_pss[i].pss_ctrl = aml_val2int(ctx,
		    res.v_package[i]->v_package[4]);
		sc->sc_pss[i].pss_status = aml_val2int(ctx,
		    res.v_package[i]->v_package[5]);
	}

	sc->sc_pss_len = res.length;

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
