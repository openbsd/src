/*	$OpenBSD: if_tht.c,v 1.6 2007/04/16 13:30:45 dlg Exp $ */

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

/*
 * Driver for the Tehuti TN30xx multi port 10Gb Ethernet chipsets,
 * see http://www.tehutinetworks.net/.
 *
 * This driver was made possible because Tehuti networks provided
 * hardware and documentation. Thanks!
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

/* registers */

#define THT_PCI_BAR		0x10

#define THT_REG_10G_REV		0x6000
#define THT_REG_10G_SCR		0x6004
#define THT_REG_10G_CTL		0x6008
#define THT_REG_10G_FMT_LNG	0x6014
#define THT_REG_10G_PAUSE	0x6018
#define THT_REG_10G_RX_SEC	0x601c
#define THT_REG_10G_TX_SEC	0x6020
#define THT_REG_10G_RFIFO_AEF	0x6024
#define THT_REG_10G_TFIFO_AEF	0x6028
#define THT_REG_10G_SM_STAT	0x6030
#define THT_REG_10G_SM_CMD	0x6034
#define THT_REG_10G_SM_DAT	0x6038
#define THT_REG_10G_SM_ADD	0x603c
#define THT_REG_10G_STAT	0x6040

/* pci controller autoconf glue */

struct thtc_softc {
	struct device		sc_dev;

	void			*sc_ih;

	bus_dma_tag_t		sc_dmat;

	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_size_t		sc_mems;
};

int			thtc_match(struct device *, void *, void *);
void			thtc_attach(struct device *, struct device *, void *);
int			thtc_print(void *, const char *);

struct cfattach thtc_ca = {
	sizeof(struct thtc_softc), thtc_match, thtc_attach
};

struct cfdriver thtc_cd = {
	NULL, "thtc", DV_DULL
};

/* port autoconf glue */

struct tht_attach_args {
	int			taa_port;
};

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
int			thtc_intr(void *);

#define DEVNAME(_sc)	((_sc)->sc_dev.dv_xname)
#define sizeofa(_a)	(sizeof(_a) / sizeof((_a)[0]))

struct thtc_device {
	pci_vendor_id_t		td_vendor;
	pci_vendor_id_t		td_product;
	u_int			td_nports;
};

const struct thtc_device *thtc_lookup(struct pci_attach_args *);

static const struct thtc_device thtc_devices[] = {
	{ PCI_VENDOR_TEHUTI,	PCI_PRODUCT_TEHUTI_TN3009, 1 },
	{ PCI_VENDOR_TEHUTI,	PCI_PRODUCT_TEHUTI_TN3010, 1 },
	{ PCI_VENDOR_TEHUTI,	PCI_PRODUCT_TEHUTI_TN3014, 2 }
};

const struct thtc_device *
thtc_lookup(struct pci_attach_args *pa)
{
	int				i;
	const struct thtc_device	*td;

	for (i = 0; i < sizeofa(thtc_devices); i++) {
		td = &thtc_devices[i];
		if (td->td_vendor == PCI_VENDOR(pa->pa_id) &&
		    td->td_product == PCI_PRODUCT(pa->pa_id))
			return (td);
	}

	return (NULL);
}

int
thtc_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args		*pa = aux;

	if (thtc_lookup(pa) != NULL)
		return (1);

	return (0);
}

void
thtc_attach(struct device *parent, struct device *self, void *aux)
{
	struct thtc_softc		*sc = (struct thtc_softc *)self;
	struct pci_attach_args		*pa = aux;
	pcireg_t			memtype;
	pci_intr_handle_t		ih;
	const char			*intrstr;
	const struct thtc_device	*td;
	struct tht_attach_args		taa;
	int				i;

	td = thtc_lookup(pa);

	sc->sc_dmat = pa->pa_dmat;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, THT_PCI_BAR);
	if (pci_mapreg_map(pa, THT_PCI_BAR, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_mems, 0) != 0) {
		printf(": unable to map host registers\n");
		return;
	}

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_NET,
	    thtc_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf(": unable to map interrupt%s%s\n",
		    intrstr == NULL ? "" : " at ",
		    intrstr == NULL ? "" : intrstr);
		goto unmap;
	}
	printf(": %s\n", intrstr);

	for (i = 0; i < td->td_nports; i++) {
		bzero(&taa, sizeof(taa));

		taa.taa_port = i;
		config_found(self, &taa, thtc_print);
	}

	return;

unmap:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

int
thtc_print(void *aux, const char *pnp)
{
	struct tht_attach_args		*taa = aux;

	if (pnp != NULL)
		printf("\"%s\" at %s", tht_cd.cd_name, pnp);

	printf(" port %d", taa->taa_port);

	return (UNCONF);
}

int
tht_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
tht_attach(struct device *parent, struct device *self, void *aux)
{
	printf("\n");
}

int
thtc_intr(void *arg)
{
	return (0);
}
