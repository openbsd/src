/*	$OpenBSD: sli_pci.c,v 1.1 2007/05/15 01:00:15 dlg Exp $ */

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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ic/slireg.h>
#include <dev/ic/slivar.h>

int	sli_pci_match(struct device *, void *, void *);
void	sli_pci_attach(struct device *, struct device *, void *);
int	sli_pci_detach(struct device *, int);

struct sli_pci_softc {
	struct sli_softc	psc_sli;

	pci_chipset_tag_t	psc_pc;
	pcitag_t		psc_tag;

	void			*psc_ih;
};

struct cfattach sli_pci_ca = {
	sizeof(struct sli_pci_softc),
	sli_pci_match,
	sli_pci_attach,
	sli_pci_detach
};

static const struct pci_matchid sli_pci_devices[] = {
	{ PCI_VENDOR_EMULEX,	PCI_PRODUCT_EMULEX_LP8000 }
};

int
sli_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, sli_pci_devices,
	    sizeof(sli_pci_devices) / sizeof(sli_pci_devices[0])));
}

void
sli_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct sli_pci_softc		*psc = (struct sli_pci_softc *)self;
	struct sli_softc		*sc = &psc->psc_sli;

	sli_attach(sc);
}

int
sli_pci_detach(struct device *self, int flags)
{
	struct sli_pci_softc		*psc = (struct sli_pci_softc *)self;
	struct sli_softc		*sc = &psc->psc_sli;
	int				rv;

	rv = sli_detach(sc, flags);
	if (rv != 0)
		return (rv);

	return (0);
}
