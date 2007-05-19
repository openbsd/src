/*	$OpenBSD: sli_pci.c,v 1.4 2007/05/19 04:10:20 dlg Exp $ */

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
	struct pci_attach_args		*pa = aux;
	pcireg_t			memtype;
	pci_intr_handle_t		ih;

	psc->psc_pc = pa->pa_pc;
	psc->psc_tag = pa->pa_tag;
	psc->psc_ih = NULL;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_ios_slim = 0;
	sc->sc_ios_reg = 0;

	memtype = pci_mapreg_type(psc->psc_pc, psc->psc_tag,
	    SLI_PCI_BAR_SLIM);
	if (pci_mapreg_map(pa, SLI_PCI_BAR_SLIM, memtype, 0,
	    &sc->sc_iot_slim, &sc->sc_ioh_slim, NULL,
	    &sc->sc_ios_slim, 0) != 0) {
		printf(": unable to map SLIM bar\n");
		return;
	}

	memtype = pci_mapreg_type(psc->psc_pc, psc->psc_tag,
	    SLI_PCI_BAR_REGISTER);
	if (pci_mapreg_map(pa, SLI_PCI_BAR_REGISTER, memtype, 0,
	    &sc->sc_iot_reg, &sc->sc_ioh_reg, NULL,
	    &sc->sc_ios_reg, 0) != 0) {
		printf(": unable to map REGISTER bar\n");
		goto unmap_slim;
	}

	if (pci_intr_map(pa, &ih)) {
		printf(": unable to map interrupt\n");
		goto unmap_reg;
	}
	printf(": %s", pci_intr_string(psc->psc_pc, ih));

	if (sli_attach(sc) != 0) {
		/* error already printed by sli_attach() */
		goto unmap_reg;
	}

	psc->psc_ih = pci_intr_establish(psc->psc_pc, ih, IPL_BIO,
	    sli_intr, sc, DEVNAME(sc));
	if (psc->psc_ih == NULL) {
		printf("%s: unable to establish interrupt\n");
		goto detach;
	}

	return;

detach:
	sli_detach(sc, DETACH_FORCE|DETACH_QUIET);
unmap_reg:
	bus_space_unmap(sc->sc_iot_reg, sc->sc_ioh_reg, sc->sc_ios_reg);
	sc->sc_ios_reg = 0;
unmap_slim:
	bus_space_unmap(sc->sc_iot_slim, sc->sc_ioh_slim, sc->sc_ios_slim);
	sc->sc_ios_slim = 0;
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
