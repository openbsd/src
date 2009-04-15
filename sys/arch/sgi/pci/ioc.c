/*	$OpenBSD: ioc.c,v 1.5 2009/04/15 18:41:32 miod Exp $	*/

/*
 * Copyright (c) 2008 Joel Sing.
 * Copyright (c) 2008, 2009 Miodrag Vallat.
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

#include <mips64/archtype.h>
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

#if 0
#include <sgi/dev/if_efreg.h>
#endif
#include <sgi/dev/owmacvar.h>

#include <sgi/xbow/xbow.h>

int	ioc_match(struct device *, void *, void *);
void	ioc_attach(struct device *, struct device *, void *);
int	ioc_search_onewire(struct device *, void *, void *);
int	ioc_search_mundane(struct device *, void *, void *);
int	ioc_print(void *, const char *);

struct ioc_intr {
	struct ioc_softc	*ii_ioc;

	int			 (*ii_func)(void *);
	void			*ii_arg;

	struct evcount		 ii_count;
	int			 ii_level;
};

struct ioc_softc {
	struct device		 sc_dev;

	struct mips_bus_space	*sc_mem_bus_space;

	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_dma_tag_t		 sc_dmat;
	pci_chipset_tag_t	 sc_pc;

	void			*sc_sih;	/* SuperIO interrupt */
	void			*sc_eih;	/* Ethernet interrupt */
	struct ioc_intr		*sc_intr[IOC_NDEVS];

	struct onewire_bus	 sc_bus;

	struct owmac_softc	*sc_owmac;
};

struct cfattach ioc_ca = {
	sizeof(struct ioc_softc), ioc_match, ioc_attach,
};

struct cfdriver ioc_cd = {
	NULL, "ioc", DV_DULL,
};

void	ioc_intr_dispatch(struct ioc_softc *, int);
int	ioc_intr_ethernet(void *);
int	ioc_intr_superio(void *);

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
	if (iaa->iaa_dev != 0)
		printf(" dev %d", iaa->iaa_dev);

	return (UNCONF);
}

void
ioc_attach(struct device *parent, struct device *self, void *aux)
{
	struct ioc_softc *sc = (struct ioc_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t sih, eih;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_size_t memsize;
	uint32_t data;
	int dev;

	if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM, 0,
	    &memt, &memh, NULL, &memsize, 0)) {
		printf(": can't map mem space\n");
		return;
	}

	sc->sc_pc = pa->pa_pc;
	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Initialise IOC3 ASIC. 
	 */
	data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	data |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_PARITY_ENABLE |
	    PCI_COMMAND_SERR_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, data);

	/*
	 * IOC3 is not a real PCI device - it's a poor wrapper over a set
	 * of convenience chips. And it actually needs to use two interrupts,
	 * one for the superio chip, and the other for the Ethernet chip.
	 *
	 * Since our pci layer doesn't handle this, we cheat and compute
	 * the superio interrupt cookie ourselves. This is ugly, and
	 * depends on xbridge knowledge.
	 *
	 * (What the above means is that you should wear peril-sensitive
	 * sunglasses from now on).
	 *
	 * To make things ever worse, some IOC3 boards (real boards, not
	 * on-board components) lack the Ethernet component. We should
	 * eventually handle them there, but it's not worth doing yet...
	 * (and we'll need to parse the ownum serial numbers to know
	 * this anyway)
	 */

	if (pci_intr_map(pa, &eih) != 0) {
		printf(": failed to map interrupt!\n");
		goto unmap;
	}

	/*
	 * The second vector source seems to be the next unused PCI
	 * slot.
	 * On Octane systems, the on-board IOC3 is device #2 and
	 * immediately followed by the RAD1 audio, device #3, thus
	 * the next empty slot is #4.
	 * XXX Is this still true with the Octane PCI cardcage?
	 * On Origin systems, there is no RAD1 audio, slot #3 is
	 * empty (available PCI slots are #5-#7).
	 */
	if (sys_config.system_type == SGI_OCTANE)
		sih = eih + 2;	/* XXX ACK GAG BARF */
	else
		sih = eih + 1;	/* XXX ACK GAG BARF */

	/*
	 * Register the superio interrupt.
	 */
	sc->sc_sih = pci_intr_establish(sc->sc_pc, sih, IPL_TTY,
	    ioc_intr_superio, sc, self->dv_xname);
	if (sc->sc_sih == NULL) {
		printf(": failed to establish superio interrupt!\n");
		goto unmap;
	}
	printf(": superio %s", pci_intr_string(sc->sc_pc, sih));

	/*
	 * Register the ethernet interrupt.
	 */
	sc->sc_eih = pci_intr_establish(sc->sc_pc, eih, IPL_NET,
	    ioc_intr_ethernet, sc, self->dv_xname);
	if (sc->sc_eih == NULL) {
		printf("\n%s: failed to establish ethernet interrupt!\n",
		    self->dv_xname);
		goto unregister;
	}
	printf(", ethernet %s\n", pci_intr_string(sc->sc_pc, eih));

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
		printf("%s: can't allocate bus_space\n", self->dv_xname);
		goto unregister2;
	}

	bcopy(memt, sc->sc_mem_bus_space, sizeof(*sc->sc_mem_bus_space));
	sc->sc_mem_bus_space->bus_base = memh;
	sc->sc_mem_bus_space->_space_read_1 = xbow_read_1;
	sc->sc_mem_bus_space->_space_read_2 = xbow_read_2;
	sc->sc_mem_bus_space->_space_write_1 = xbow_write_1;
	sc->sc_mem_bus_space->_space_write_2 = xbow_write_2;

	/* XXX undo IP27 xbridge weird mapping */
	if (sys_config.system_type != SGI_OCTANE)
		sc->sc_mem_bus_space->_space_map = xbow_space_map_short;

	sc->sc_memt = sc->sc_mem_bus_space;
	sc->sc_memh = memh;

	/* Initialise interrupt handling structures. */
	for (dev = 0; dev < IOC_NDEVS; dev++)
		sc->sc_intr[dev] = NULL;

	/*
	 * Acknowledge all pending interrupts, and disable them.
	 */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IEC, ~0x0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IES, 0x0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IR,
	    bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IR));

	/*
	 * Attach the 1-Wire interface first, other sub-devices may
	 * need the information they'll provide.
	 */
	config_search(ioc_search_onewire, self, aux);

	/*
	 * Attach other sub-devices.
	 */
	config_search(ioc_search_mundane, self, aux);

	return;

unregister2:
	pci_intr_disestablish(sc->sc_pc, sc->sc_eih);
unregister:
	pci_intr_disestablish(sc->sc_pc, sc->sc_sih);
unmap:
	bus_space_unmap(memt, memh, memsize);
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
		iaa.iaa_dev = 0;
	else
		iaa.iaa_dev = cf->cf_loc[1];

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

	/* In case onewire is disabled in UKC... */
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

/*
 * Interrupt handling.
 */

/*
 * List of interrupt bits to enable for each device.
 *
 * For the serial ports, we only enable the passthrough interrupt and
 * let com(4) tinker with the appropriate registers, instead of adding
 * an unnecessary layer there.
 */
const uint32_t ioc_intrbits[IOC_NDEVS] = {
	0x00000040,	/* serial A */
	0x00008000,	/* serial B */
	0x00040000,	/* parallel port */
	0x00400000,	/* PS/2 port */
	0x08000000,	/* rtc */
	0x00000000	/* Ethernet (handled differently) */
};

void *
ioc_intr_establish(void *cookie, u_long dev, int level, int (*func)(void *),
    void *arg, char *name)
{
        struct ioc_softc *sc = cookie;
	struct ioc_intr *ii;

	dev--;
	if (dev < 0 || dev >= IOC_NDEVS)
		return NULL;

	ii = (struct ioc_intr *)malloc(sizeof(*ii), M_DEVBUF, M_NOWAIT);
	if (ii == NULL)
		return NULL;

	ii->ii_ioc = sc;
	ii->ii_func = func;
	ii->ii_arg = arg;
	ii->ii_level = level;

	evcount_attach(&ii->ii_count, name, &ii->ii_level, &evcount_intr);
	sc->sc_intr[dev] = ii;

	/* enable hardware source if necessary */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IES,
	    ioc_intrbits[dev]);
	
	return (ii);
}

int
ioc_intr_superio(void *v)
{
	struct ioc_softc *sc = (struct ioc_softc *)v;
	uint32_t pending;
	int dev;

	pending = bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IR) &
	    bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IES);

	if (pending == 0)
		return 0;

	/* Disable pending interrupts */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IEC, pending);

	for (dev = 0; dev < IOC_NDEVS - 1 /* skip Ethernet */; dev++) {
		if (pending & ioc_intrbits[dev]) {
			ioc_intr_dispatch(sc, dev);

			/* Ack, then reenable, pending interrupts */
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    IOC3_SIO_IR, pending & ioc_intrbits[dev]);
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    IOC3_SIO_IES, pending & ioc_intrbits[dev]);
		}
	}

	return 1;
}

int
ioc_intr_ethernet(void *v)
{
#if 0
	struct ioc_softc *sc = (struct ioc_softc *)v;
	uint32_t stat;

	stat = bus_space_read_4(sc->sc_memt, sc->sc_memh, EF_INTR_STATUS);

	if (stat == 0)
		return 0;

	ioc_intr_dispatch(sc, IOCDEV_EF);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, EF_INTR_STATUS, stat);

	return 1;
#else
	return 0;
#endif
}

void
ioc_intr_dispatch(struct ioc_softc *sc, int dev)
{
	struct ioc_intr *ii;

	/* Call registered interrupt function. */
	if ((ii = sc->sc_intr[dev]) != NULL && ii->ii_func != NULL) {
		if ((*ii->ii_func)(ii->ii_arg) != 0)
               		ii->ii_count.ec_count++;
	}
}
