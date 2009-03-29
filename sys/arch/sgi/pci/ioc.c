/*	$OpenBSD: ioc.c,v 1.2 2009/03/29 21:53:52 sthen Exp $	*/

/*
 * Copyright (c) 2008 Joel Sing.
 * Copyright (c) 2008 Miodrag Vallat.
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
 * IOC device driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <sgi/pci/iocreg.h>
#include <sgi/pci/iocvar.h>

#include <dev/onewire/onewirereg.h>
#include <dev/onewire/onewirevar.h>

#include <sgi/dev/owmacvar.h>

#include <sgi/xbow/xbow.h>

int	ioc_match(struct device *, void *, void *);
void	ioc_attach(struct device *, struct device *, void *);
int	ioc_search_onewire(struct device *, void *, void *);
int	ioc_search_mundane(struct device *, void *, void *);
int	ioc_print(void *, const char *);

struct ioc_softc {
	struct device		 sc_dev;

	struct mips_bus_space	*sc_mem_bus_space;

	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_dma_tag_t		 sc_dmat;

	struct onewire_bus	 sc_bus;

	struct owmac_softc	*sc_owmac;
};

struct cfattach ioc_ca = {
	sizeof(struct ioc_softc), ioc_match, ioc_attach,
};

struct cfdriver ioc_cd = {
	NULL, "ioc", DV_DULL,
};

int	iocow_reset(void *);
int	iocow_read_bit(struct ioc_softc *);
int	iocow_send_bit(void *, int);
int	iocow_read_byte(void *);
int	iocow_triplet(void *, int);
int	iocow_pulse(struct ioc_softc *, int, int);

int
ioc_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SGI &&
            PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SGI_IOC3)
		return (1);

	return (0);
}

int
ioc_print(void *aux, const char *iocname)
{
	struct ioc_attach_args *iaa = aux;

	if (iocname != NULL)
		printf("%s at %s", iaa->iaa_name, iocname);

	if (iaa->iaa_base != 0)
		printf(" base 0x%08x", iaa->iaa_base);
	if (iaa->iaa_intr != 0)
		printf(" irq %d", iaa->iaa_intr);

	return (UNCONF);
}

void
ioc_attach(struct device *parent, struct device *self, void *aux)
{
	struct ioc_softc *sc = (struct ioc_softc *)self;
	struct pci_attach_args *pa = aux;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_size_t memsize;

	if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM, 0,
	    &memt, &memh, NULL, &memsize, 0)) {
		printf(": can't map mem space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Build a suitable bus_space_handle by rebasing the xbridge
	 * inherited one to our BAR, and restoring the original
	 * non-swapped subword access methods.
	 *
	 * XXX This is horrible and will need to be rethought if
	 * XXX we ever support ioc3 cards not plugged to xbridges.
	 */

	sc->sc_mem_bus_space = malloc(sizeof (*sc->sc_mem_bus_space),
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_mem_bus_space == NULL) {
		bus_space_unmap(memt, memh, memsize);
		printf(": can't allocate bus_space\n");
		return;
	}

	bcopy(memt, sc->sc_mem_bus_space, sizeof(*sc->sc_mem_bus_space));
	sc->sc_mem_bus_space->bus_base = memh;
	sc->sc_mem_bus_space->_space_read_1 = xbow_read_1;
	sc->sc_mem_bus_space->_space_read_2 = xbow_read_2;
	sc->sc_mem_bus_space->_space_write_1 = xbow_write_1;
	sc->sc_mem_bus_space->_space_write_2 = xbow_write_2;

	sc->sc_memt = sc->sc_mem_bus_space;
	sc->sc_memh = memh;

	printf("\n");

	/*
	 * Attach the 1-Wire interface first, other sub-devices may
	 * need the information they'll provide.
	 */
	config_search(ioc_search_onewire, self, aux);

	/*
	 * Attach other sub-devices.
	 */
	config_search(ioc_search_mundane, self, aux);
}

int
ioc_search_mundane(struct device *parent, void *vcf, void *args)
{
	struct ioc_softc *sc = (struct ioc_softc *)parent;
	struct cfdata *cf = vcf;
	struct ioc_attach_args iaa;

	if (strcmp(cf->cf_driver->cd_name, "onewire") == 0)
		return 0;

	iaa.iaa_name = cf->cf_driver->cd_name;
	iaa.iaa_memt = sc->sc_memt;
	iaa.iaa_dmat = sc->sc_dmat;

	if (cf->cf_loc[0] == -1)
		iaa.iaa_base = 0;
	else
		iaa.iaa_base = cf->cf_loc[0];
	if (cf->cf_loc[1] == -1)
		iaa.iaa_intr = 0;
	else
		iaa.iaa_intr = cf->cf_loc[1];

	if (sc->sc_owmac != NULL)
		memcpy(iaa.iaa_enaddr, sc->sc_owmac->sc_enaddr, 6);
	else
		bzero(iaa.iaa_enaddr, 6);

        if ((*cf->cf_attach->ca_match)(parent, cf, &iaa) == 0)
                return 0;

	config_attach(parent, cf, &iaa, ioc_print);
	return 1;
}

/*
 * Number-In-a-Can access driver (1-Wire interface through IOC)
 */

int
ioc_search_onewire(struct device *parent, void *vcf, void *args)
{
	struct ioc_softc *sc = (struct ioc_softc *)parent;
	struct cfdata *cf = vcf;
	struct onewirebus_attach_args oba;
	struct device *owdev, *dev;
	extern struct cfdriver owmac_cd;

	if (strcmp(cf->cf_driver->cd_name, "onewire") != 0)
		return 0;

	sc->sc_bus.bus_cookie = sc;
	sc->sc_bus.bus_reset = iocow_reset;
	sc->sc_bus.bus_bit = iocow_send_bit;
	sc->sc_bus.bus_read_byte = iocow_read_byte;
	sc->sc_bus.bus_write_byte = NULL;	/* use default routine */
	sc->sc_bus.bus_read_block = NULL;	/* use default routine */
	sc->sc_bus.bus_write_block = NULL;	/* use default routine */
	sc->sc_bus.bus_triplet = iocow_triplet;
	sc->sc_bus.bus_matchrom = NULL;		/* use default routine */
	sc->sc_bus.bus_search = NULL;		/* use default routine */

	oba.oba_bus = &sc->sc_bus;
	oba.oba_flags = ONEWIRE_SCAN_NOW | ONEWIRE_NO_PERIODIC_SCAN;

	/* in case onewire is disabled in UKC */
        if ((*cf->cf_attach->ca_match)(parent, cf, &oba) == 0)
                return 0;

	owdev = config_attach(parent, cf, &oba, onewirebus_print);

	/*
	 * Find the first owmac child of the onewire bus, and keep
	 * a pointer to it.  This allows us to pass the ethernet
	 * address to the ethernet subdevice.
	 */
	if (owdev != NULL) {
		TAILQ_FOREACH(dev, &alldevs, dv_list)
			if (dev->dv_parent == owdev &&
			    dev->dv_cfdata->cf_driver == &owmac_cd) {
				sc->sc_owmac = (struct owmac_softc *)dev;
				break;
			}
	}
	return 1;
}

int
iocow_reset(void *v)
{
	struct ioc_softc *sc = v;
	return iocow_pulse(sc, 500, 65);
}

int
iocow_read_bit(struct ioc_softc *sc)
{
	return iocow_pulse(sc, 6, 13);
}

int
iocow_send_bit(void *v, int bit)
{
	struct ioc_softc *sc = v;
	int rc;
	
	if (bit != 0)
		rc = iocow_pulse(sc, 6, 110);
	else
		rc = iocow_pulse(sc, 80, 30);
	return rc;
}

int
iocow_read_byte(void *v)
{
	struct ioc_softc *sc = v;
	unsigned int byte = 0;
	int i;

	for (i = 0; i < 8; i++)
		byte |= iocow_read_bit(sc) << i;

	return byte;
}

int
iocow_triplet(void *v, int dir)
{
	struct ioc_softc *sc = v;
	int rc;

	rc = iocow_read_bit(sc);
	rc <<= 1;
	rc |= iocow_read_bit(sc);

	switch (rc) {
	case 0x0:
		iocow_send_bit(v, dir);
		break;
	case 0x1:
		iocow_send_bit(v, 0);
		break;
	default:
		iocow_send_bit(v, 1);
		break;
	}

	return (rc);
}

int
iocow_pulse(struct ioc_softc *sc, int pulse, int data)
{
	uint32_t mcr_value;

	mcr_value = (pulse << 10) | (data << 2);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_MCR, mcr_value);
	do {
		mcr_value =
		    bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC3_MCR);
	} while ((mcr_value & 0x00000002) == 0);

	delay(500);

	return (mcr_value & 1);
}
