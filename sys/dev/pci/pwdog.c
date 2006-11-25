/*	$OpenBSD: pwdog.c,v 1.2 2006/11/25 18:06:01 mbalmer Exp $ */

/*
 * Copyright (c) 2006 Marc Balmer <mbalmer@openbsd.org>
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

struct pwdog_softc {
	struct device		pwdog_dev;
	bus_space_tag_t		iot;
	bus_space_handle_t	ioh;
};

/* registers */
#define PWDOG_ACTIVATE	0
#define PWDOG_DISABLE	1

int pwdog_probe(struct device *, void *, void *);
void pwdog_attach(struct device *, struct device *, void *);
void pwdog_init_timer(struct pwdog_softc *);
int pwdog_set_timeout(void *, int);
void pwdog_disable_timer(struct pwdog_softc *);

struct cfattach pwdog_ca = {
	sizeof(struct pwdog_softc), pwdog_probe, pwdog_attach
};

struct cfdriver pwdog_cd = {
	NULL, "pwdog", DV_DULL
};

int
pwdog_probe(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *const pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_QUANCOM ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_QUANCOM_PWDOG1)
		return 0;
	return 1;
}

void
pwdog_attach(struct device *parent, struct device *self, void *aux)
{
	struct pwdog_softc *pwdog = (struct pwdog_softc *)self;
	struct pci_attach_args *const pa = (struct pci_attach_args *)aux;
	pcireg_t memtype;
	bus_size_t iosize;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START);
	if (pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0, &pwdog->iot,
	    &pwdog->ioh, NULL, &iosize, 0)) {
		printf("\n%s: PCI %s region not found\n",
		    pwdog->pwdog_dev.dv_xname,
		    memtype == PCI_MAPREG_TYPE_IO ? "I/O" : "memory");
		return;
	}
	printf("\n");
	pwdog_disable_timer(pwdog);
	wdog_register(pwdog, pwdog_set_timeout);
}

/* ARGSUSED */
int
pwdog_set_timeout(void *self, int seconds)
{
	struct pwdog_softc *pwdog = (struct pwdog_softc *)self;
	int s;

	s = splclock();
	pwdog_disable_timer(pwdog);
	if (!seconds) {
		splx(s);
		return 0;
	}
	bus_space_write_1(pwdog->iot, pwdog->ioh, PWDOG_ACTIVATE, 0);
	splx(s);
	return seconds;
}

void
pwdog_disable_timer(struct pwdog_softc *pwdog)
{
	bus_space_write_1(pwdog->iot, pwdog->ioh, PWDOG_DISABLE, 0);
}
