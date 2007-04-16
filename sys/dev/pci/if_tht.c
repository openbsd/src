/*	$OpenBSD: if_tht.c,v 1.1 2007/04/16 10:35:29 dlg Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/* pci controller autoconf glue */

struct thtc_softc {
	struct device		csc_dev;

	void			*sc_ih;
};

int			thtc_match(struct device *, void *, void *);
void			thtc_attach(struct device *, struct device *, void *);

struct cfattach thtc_ca = {
	sizeof(struct thtc_softc), thtc_match, thtc_attach
};

struct cfdriver thtc_cd = {
	NULL, "thtc", DV_DULL
};

/* port autoconf glue */

struct tht_softc {
	struct device		csc_dev;
};

int			tht_match(struct device *, void *, void *);
void			tht_attach(struct device *, struct device *, void *);

struct cfattach tht_ca = {
	sizeof(struct tht_softc), tht_match, tht_attach
};

struct cfdriver tht_cd = {
	NULL, "tht", DV_IFNET
};

/* misc goo */

#define DEVNAME(_sc)	((_sc)->sc_dev.dv_xname)
#define sizeofa(_a)	(sizeof(_a) / sizeof((_a)[0]))

static const struct pci_matchid thtc_devices[] = {
	{ PCI_VENDOR_TEHUTI,	PCI_PRODUCT_TEHUTI_TN3009 },
	{ PCI_VENDOR_TEHUTI,	PCI_PRODUCT_TEHUTI_TN3010 },
	{ PCI_VENDOR_TEHUTI,	PCI_PRODUCT_TEHUTI_TN3014 }
};

int
thtc_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args		*pa = aux;

	return (pci_matchbyid(pa, thtc_devices, sizeofa(thtc_devices)));
}

void
thtc_attach(struct device *parent, struct device *self, void *aux)
{
	printf("\n");
}

int
tht_match(struct device *parent, void *match, void *aux)
{
	return (0);
}

void
tht_attach(struct device *parent, struct device *self, void *aux)
{
	printf("\n");
}
