/* $OpenBSD: ispi_acpi.c,v 1.1 2025/11/14 01:55:07 jcs Exp $ */
/*
 * Intel LPSS SPI controller
 * ACPI attachment
 *
 * Copyright (c) 2015-2019 joshua stein <jcs@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
#include <sys/kernel.h>
#include <sys/kthread.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/ic/ispivar.h>

int	ispi_acpi_match(struct device *, void *, void *);
void	ispi_acpi_attach(struct device *, struct device *, void *);

int	ispi_activate(struct device *, int);
void	ispi_acpi_bus_scan(struct ispi_softc *);

int	ispi_configure(void *, int, int, int);
void	ispi_start(struct ispi_softc *);
void	ispi_send(struct ispi_softc *);
void	ispi_recv(struct ispi_softc *);

const struct cfattach ispi_acpi_ca = {
	sizeof(struct ispi_softc),
	ispi_acpi_match,
	ispi_acpi_attach,
	NULL,
	ispi_activate,
};

const char *ispi_acpi_hids[] = {
	"INT33C0",
	"INT33C1",
	"INT3430",
	"INT3431",
	NULL
};

int
ispi_acpi_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, ispi_acpi_hids, cf->cf_driver->cd_name);
}

void
ispi_acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct ispi_softc *sc = (struct ispi_softc *)self;
	struct acpi_attach_args *aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;
	rw_init(&sc->sc_buslock, sc->sc_dev.dv_xname);

	printf(": %s", sc->sc_devnode->name);

	ispi_init(sc);
}

void
ispi_acpi_bus_scan(struct ispi_softc *sc)
{
	aml_find_node(sc->sc_devnode, "_HID", ispi_acpi_found_hid, sc);
}

int
ispi_acpi_found_hid(struct aml_node *node, void *arg)
{
	struct ispi_softc *sc = (struct ispi_softc *)arg;
	int64_t sta;
	char cdev[32], dev[32];

	if (node->parent == sc->sc_devnode)
		return 0;

	if (acpi_parsehid(node, arg, cdev, dev, sizeof(cdev)) != 0)
		return 0;

	sta = acpi_getsta(acpi_softc, node->parent);
	if ((sta & STA_PRESENT) == 0)
		return 0;

	DPRINTF(("%s: found HID %s at %s\n", sc->sc_dev.dv_xname, dev,
	    aml_nodename(node)));

	acpi_attach_deps(acpi_softc, node->parent);

	/* TODO */

	return 0;
}
