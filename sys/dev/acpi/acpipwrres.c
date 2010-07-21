/* $OpenBSD: acpipwrres.c,v 1.4 2010/07/21 19:35:15 deraadt Exp $ */
/*
 * Copyright (c) 2009 Paul Irofti <pirofti@openbsd.org>
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

int	acpipwrres_match(struct device *, void *, void *);
void	acpipwrres_attach(struct device *, struct device *, void *);
int	acpipwrres_notify(struct aml_node *, int, void *);

#define	NOTIFY_PWRRES_OFF	0
#define NOTIFY_PWRRES_ON	1

#ifdef ACPIPWRRES_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

struct acpipwrres_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	TAILQ_HEAD(acpipwrres_cons_h, acpipwrres_consumer)	sc_cons;

	int	sc_level;
	int	sc_order;
	int	sc_ncons;
	int	sc_state;
#define ACPIPWRRES_OFF		0
#define ACPIPWRRES_ON		1
#define ACPIPWRRES_UNK		-1
};

struct acpipwrres_consumer {
	struct aml_node				*cs_node;
	TAILQ_ENTRY(acpipwrres_consumer)	cs_link;
};

struct cfattach acpipwrres_ca = {
	sizeof(struct acpipwrres_softc), acpipwrres_match, acpipwrres_attach
};

struct cfdriver acpipwrres_cd = {
	NULL, "acpipwrres", DV_DULL
};

int	acpipwrres_foundcons(struct aml_node *, void *);

int
acpipwrres_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata		*cf = match;

	if (aaa->aaa_name == NULL || strcmp(aaa->aaa_name,
	    cf->cf_driver->cd_name) != 0 || aaa->aaa_table != NULL)
		return (0);

	return (1);
}

void
acpipwrres_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpipwrres_softc	*sc = (struct acpipwrres_softc *)self;
	struct acpi_attach_args *aaa = aux;
	struct aml_value	res;

	extern struct aml_node	aml_root;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aaa->aaa_node;
	memset(&res, 0, sizeof res);

	printf(": %s\n", sc->sc_devnode->name);

	aml_register_notify(sc->sc_devnode, aaa->aaa_dev,
	    acpipwrres_notify, sc, ACPIDEV_NOPOLL);

	if (!aml_evalname(sc->sc_acpi, sc->sc_devnode, "_STA", 0, NULL, &res)) {
		sc->sc_state = (int)aml_val2int(&res);
		if (sc->sc_state != ACPIPWRRES_ON &&
		    sc->sc_state != ACPIPWRRES_OFF)
			sc->sc_state = ACPIPWRRES_UNK;
	} else
		sc->sc_state = ACPIPWRRES_UNK;
	DPRINTF(("%s: state = %d\n", DEVNAME(sc), sc->sc_state));
	if (aml_evalnode(sc->sc_acpi, aaa->aaa_node, 0, NULL, &res) == 0) {
		sc->sc_level = res.v_powerrsrc.pwr_level;
		sc->sc_order = res.v_powerrsrc.pwr_order;
		DPRINTF(("%s: level = %d, order %d\n", DEVNAME(sc),
		    sc->sc_level, sc->sc_order));
		aml_freevalue(&res);
	}

	/* Get the list of consumers */
	TAILQ_INIT(&sc->sc_cons);
	aml_find_node(&aml_root, "_PRW", acpipwrres_foundcons, sc);
	aml_find_node(&aml_root, "_PR0", acpipwrres_foundcons, sc);
	aml_find_node(&aml_root, "_PR1", acpipwrres_foundcons, sc);
	aml_find_node(&aml_root, "_PR2", acpipwrres_foundcons, sc);
}

int
acpipwrres_notify(struct aml_node *node, int notify, void *arg)
{
	int				fmatch = 0;
	struct acpipwrres_consumer	*cons;
	struct acpipwrres_softc		*sc = arg;
	struct aml_value		res;

	memset(&res, 0, sizeof res);

	TAILQ_FOREACH(cons, &sc->sc_cons, cs_link)
		if (cons->cs_node == node) {
			fmatch = 1;
			break;
		}
	if (!fmatch)
		return (0);

	switch (notify) {
	case NOTIFY_PWRRES_ON:
		DPRINTF(("pwr: on devs %d\n", sc->sc_ncons));
		if (sc->sc_ncons++ == 0)
			aml_evalname(sc->sc_acpi, sc->sc_devnode, "_ON", 0, NULL,
			    &res);
		aml_freevalue(&res);
		break;
	case NOTIFY_PWRRES_OFF:
		DPRINTF(("pwr: off devs %d\n", sc->sc_ncons));
		if (--sc->sc_ncons == 0)
			aml_evalname(sc->sc_acpi, sc->sc_devnode, "_OFF", 0, NULL,
			    &res);
		aml_freevalue(&res);
		break;
	default:
		printf("%s: unknown event 0x%02x\n", DEVNAME(sc), notify);
		break;
	}

	return (0);
}

int
acpipwrres_foundcons(struct aml_node *node, void *arg)
{
	int				i = 0;
	struct acpipwrres_consumer	*cons;
	struct aml_node			*pnode;
	struct acpipwrres_softc		*sc = (struct acpipwrres_softc *)arg;
	struct aml_value		res;

	extern struct aml_node	aml_root;

	memset(&res, 0, sizeof res);

	if (aml_evalnode(sc->sc_acpi, node, 0, NULL, &res) == -1) {
		DPRINTF(("pwr: consumer not found\n"));
		return (-1);
	} else {
		DPRINTF(("%s: node name %s\n", DEVNAME(sc), aml_nodename(node)));
		if (!strcmp(node->name, "_PRW"))
			i = 2;          /* _PRW first two values are ints */
		for (; i < res.length; i++) {
			pnode = aml_searchname(&aml_root,
			    res.v_package[i]->v_string);
			if (pnode == sc->sc_devnode) {
				DPRINTF(("%s: consumer match\n", DEVNAME(sc)));
				cons = malloc(sizeof *cons, M_DEVBUF,
				    M_WAITOK | M_ZERO);
				cons->cs_node = pnode;
				TAILQ_INSERT_HEAD(&sc->sc_cons, cons, cs_link);
			}
		}
	}

	return (0);
}
