/*	$OpenBSD: ahci.c,v 1.2 2006/12/09 05:19:49 dlg Exp $ */

/*
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
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

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define AHCI_PCI_BAR			0x24

#define AHCI_REG_CAP			0x000 /* HBA Capabilities */
#define AHCI_REG_GHC			0x004 /* Global HBA Control */
#define AHCI_REG_IS			0x008 /* Interrupt Status */
#define AHCI_REG_PI			0x00c /* Ports Implemented */
#define AHCI_REG_VS			0x010 /* AHCI Version */
#define AHCI_REG_CCC_CTL		0x014 /* Coalescing Control */
#define AHCI_REG_CCC_PORTS		0x018 /* Coalescing Ports */
#define AHCI_REG_EM_LOC			0x01c /* Enclosure Mgmt Location */
#define AHCI_REG_EM_CTL			0x020 /* Enclosure Mgmt Control */

static const struct pci_matchid ahci_devices[] = {
	{ PCI_VENDOR_JMICRON,	PCI_PRODUCT_JMICRON_JMB361 }
};

int		ahci_match(struct device *, void *, void *);
void		ahci_attach(struct device *, struct device *, void *);

struct ahci_softc {
	struct device		sc_dev;

	void			*sc_ih;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;
};
#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

struct cfattach ahci_ca = {
	sizeof(struct ahci_softc), ahci_match, ahci_attach
};

struct cfdriver ahci_cd = {
	NULL, "ahci", DV_DULL
};

int		ahci_intr(void *);


int
ahci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, ahci_devices,
	    sizeof(ahci_devices) / sizeof(ahci_devices[0])));
}

void
ahci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ahci_softc		*sc = (struct ahci_softc *)self;
	struct pci_attach_args		*pa = aux;
	pcireg_t			memtype;
	pci_intr_handle_t		ih;
	const char			*intrstr;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, AHCI_PCI_BAR);
	if (pci_mapreg_map(pa, AHCI_PCI_BAR, memtype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &sc->sc_ios, 0) != 0) {
		printf(": unable to map registers\n");
		return;
	}

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
	    ahci_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf(": unable to map interrupt%s%s\n",
		    intrstr == NULL ? "" : " at ",
		    intrstr == NULL ? "" : intrstr);
		goto unmap;
	}
	printf(": %s\n", intrstr);

	return;

unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
}

int
ahci_intr(void *arg)
{
	return (0);
}
