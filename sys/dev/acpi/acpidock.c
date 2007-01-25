/* $OpenBSD: acpidock.c,v 1.4 2007/01/25 21:26:06 mk Exp $ */
/*
 * Copyright (c) 2006,2007 Michael Knudsen <mk@openbsd.org>
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

int	acpidock_match(struct device *, void *, void *);
void	acpidock_attach(struct device *, struct device *, void *);

struct cfattach acpidock_ca = {
	sizeof(struct acpidock_softc), acpidock_match, acpidock_attach
};

struct cfdriver acpidock_cd = {
	NULL, "acpidock", DV_DULL
};


int	acpidock_docklock(struct acpidock_softc *, int);
int	acpidock_dockctl(struct acpidock_softc *, int);
int	acpidock_eject(struct acpidock_softc *);
int	acpidock_init(struct acpidock_softc *);
int	acpidock_notify(struct aml_node *, int, void *);
int	acpidock_status(struct acpidock_softc *);

int
acpidock_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	 *aaa = aux;
	struct cfdata		 *cf = match;

	/* sanity */
	if (aaa->aaa_name == NULL ||
	    strcmp(aaa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aaa->aaa_table != NULL)
		return (0);

	return (1);
}

void
acpidock_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpidock_softc	*sc = (struct acpidock_softc *)self;
	struct acpi_attach_args *aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node->child;

	printf(": %s", sc->sc_devnode->parent->name);

	sc->sc_docked = ACPIDOCK_STATUS_UNKNOWN;

	if (!acpidock_init(sc)) {
		printf(": couldn't initialize\n");
		return;
	}

	acpidock_status(sc);
	printf(": %s\n",
	    sc->sc_docked == ACPIDOCK_STATUS_DOCKED ? "docked" : "undocked");

	if (sc->sc_docked == ACPIDOCK_STATUS_DOCKED) {
		acpidock_docklock(sc, 1);
		acpidock_dockctl(sc, 1);
	} else {
		acpidock_dockctl(sc, 0);
		acpidock_docklock(sc, 0);
	}

	aml_register_notify(sc->sc_devnode->parent, aa->aaa_dev, 
	    acpidock_notify, sc, ACPIDEV_NOPOLL);

}

int
acpidock_init(struct acpidock_softc *sc)
{
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode->parent, "_INI", 0, NULL,
	    NULL) != 0)
		return (0);
	else
		return (1);
}

int
acpidock_status(struct acpidock_softc *sc)
{
	struct aml_value	res;
	int			rv, sta;

	memset(&res, 0, sizeof res);
	/* XXX: wrong */
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode->parent, "_STA", 0, NULL,
	    &res) != 0)
		rv = 0;
	else
		rv = 1;
	
	sta = aml_val2int(&res);
	sc->sc_sta = sta;

	/* XXX: _STA bit defines */
	sc->sc_docked = sta & 0x01;

	aml_freevalue(&res);

	return (rv);
}

int
acpidock_docklock(struct acpidock_softc *sc, int lock)
{
	struct aml_value	cmd;
	struct aml_value	res;

	memset(&cmd, 0, sizeof cmd);
	cmd.v_integer = lock;
#if 1
	cmd.type = AML_OBJTYPE_INTEGER;
#endif
	memset(&res, 0, sizeof res);
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode->parent, "_LCK", 1, &cmd,
	    &res) != 0) {
		dnprintf(20, "%s: _LCD %d failed\n", DEVNAME(sc), lock);

		aml_freevalue(&res);
		return (0);
	} else {
		dnprintf(20, "%s: _LCK %d successful\n", DEVNAME(sc), lock);

		aml_freevalue(&res);
		return (1);
	}

}

int
acpidock_dockctl(struct acpidock_softc *sc, int dock)
{
	struct aml_value	cmd;
	struct aml_value	res;

	memset(&cmd, 0, sizeof cmd);
	cmd.v_integer = 1;
	cmd.type = AML_OBJTYPE_INTEGER;
	memset(&res, 0, sizeof res);

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode->parent, "_DCK", 1, &cmd,
	    &res) != 0) {
		/* XXX */
		dnprintf(15, "%s: _DCK %d failed\n", DEVNAME(sc), dock);

		sc->sc_docked = 0;

		aml_freevalue(&res);
		return (0);
	} else {
		dnprintf(15, "%s: _DCK %d successful\n", DEVNAME(sc), dock);

		sc->sc_docked = 1;
		aml_freevalue(&res);
		return (1);
	}

}

int
acpidock_eject(struct acpidock_softc *sc)
{
	struct aml_value	cmd;
	struct aml_value	res;

	memset(&cmd, 0, sizeof cmd);
	cmd.v_integer = 1;
	cmd.type = AML_OBJTYPE_INTEGER;

	memset(&res, 0, sizeof res);
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode->parent, "_EJ0", 1, &cmd,
	    &res) != 0) {
		/* XXX */
		dnprintf(15, "%s: _EJ0 %d failed\n", DEVNAME(sc), dock);

		aml_freevalue(&res);
		return (0);
	} else {
		dnprintf(15, "%s: _EJ0 %d successful\n", DEVNAME(sc), dock);

		sc->sc_docked = 0;
		aml_freevalue(&res);
		return (1);
	}

}

int
acpidock_notify(struct aml_node *node, int notify_type, void *arg)
{
	struct acpidock_softc	*sc = arg;

	printf("acpidock_notify: notify %d\n", notify_type);
	switch (notify_type) {
	case ACPIDOCK_EVENT_INSERT:
		dnprintf(10, "INSERT\n");
		acpidock_docklock(sc, 1);
		acpidock_dockctl(sc, 1);
		break;
	case ACPIDOCK_EVENT_EJECT:
		dnprintf(10, "EJECT\n");
		acpidock_dockctl(sc, 0);
		acpidock_docklock(sc, 0);

		/* now actually undock */
		acpidock_eject(sc);
		
		break;
	}

	acpidock_status(sc);
	dnprintf(5, "acpidock_notify: status %s\n",
	    sc->sc_docked == ACPIDOCK_STATUS_DOCKED ? "docked" : "undocked");

	return (0);
}

