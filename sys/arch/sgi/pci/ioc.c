/*	$OpenBSD: ioc.c,v 1.19 2009/07/26 19:58:51 miod Exp $	*/

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

#include <sgi/dev/owmacvar.h>
#include <sgi/dev/owserialvar.h>

#include <sgi/xbow/xbow.h>

int	ioc_match(struct device *, void *, void *);
void	ioc_attach(struct device *, struct device *, void *);
void	ioc_attach_child(struct device *, const char *, bus_addr_t, int);
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

	void			*sc_ih1;	/* Ethernet interrupt */
	void			*sc_ih2;	/* SuperIO interrupt */
	struct ioc_intr		*sc_intr[IOC_NDEVS];

	struct onewire_bus	 sc_bus;

	struct owmac_softc	*sc_owmac;
	struct owserial_softc	*sc_owserial;
};

struct cfattach ioc_ca = {
	sizeof(struct ioc_softc), ioc_match, ioc_attach,
};

struct cfdriver ioc_cd = {
	NULL, "ioc", DV_DULL,
};

int	ioc_intr_dispatch(struct ioc_softc *, int);
int	ioc_intr_ethernet(void *);
int	ioc_intr_shared(void *);
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

	if ((int)iaa->iaa_base > 0)
		printf(" base 0x%08x", iaa->iaa_base);

	return (UNCONF);
}

void
ioc_attach(struct device *parent, struct device *self, void *aux)
{
	struct ioc_softc *sc = (struct ioc_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih1, ih2;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_size_t memsize;
	uint32_t data;
	int dev;
	int dual_irq, shared_handler, has_ethernet, has_ps2, has_serial;

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
	data &= ~PCI_COMMAND_INTERRUPT_DISABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, data);

	printf("\n");

	/*
	 * Build a suitable bus_space_handle by restoring the original
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
	sc->sc_mem_bus_space->_space_read_1 = xbow_read_1;
	sc->sc_mem_bus_space->_space_read_2 = xbow_read_2;
	sc->sc_mem_bus_space->_space_read_raw_2 = xbow_read_raw_2;
	sc->sc_mem_bus_space->_space_write_1 = xbow_write_1;
	sc->sc_mem_bus_space->_space_write_2 = xbow_write_2;
	sc->sc_mem_bus_space->_space_write_raw_2 = xbow_write_raw_2;

	sc->sc_memt = sc->sc_mem_bus_space;
	sc->sc_memh = memh;

	/*
	 * Attach the 1-Wire bus now, so that we can get our own part
	 * number and deduce which devices are really available on the
	 * board.
	 */

	config_search(ioc_search_onewire, self, aux);

	/*
	 * Now figure out what our configuration is.
	 */

	printf("%s: ", self->dv_xname);

	dual_irq = shared_handler = 0;
	has_ethernet = has_ps2 = has_serial = 0;
	if (sc->sc_owserial != NULL) {
		if (strncmp(sc->sc_owserial->sc_product, "030-0873-", 9) == 0) {
			/* MENET board */
			has_ethernet = has_serial = 1;
			shared_handler = 1;
		} else
		if (strncmp(sc->sc_owserial->sc_product, "030-0891-", 9) == 0) {
			/* IP30 on-board IOC3 */
			has_ethernet = has_ps2 = has_serial = 1;
			dual_irq = 1;
		} else
		if (strncmp(sc->sc_owserial->sc_product, "030-1155-", 9) == 0) {
			/* CADDuo board */
			has_ps2 = has_ethernet = 1;
			shared_handler = 1;
		} else
		if (strncmp(sc->sc_owserial->sc_product, "030-1657-", 9) == 0 ||
		    strncmp(sc->sc_owserial->sc_product, "030-1664-", 9) == 0) {
			/* PCI_SIO_UFC dual serial board */
			has_serial = 1;
		} else {
			goto unknown;
		}
	} else {
		/*
		 * If no owserial device has been found, then it is
		 * very likely that we are the on-board IOC3 found
		 * on IP27 and IP35 systems.
		 */
		if (sys_config.system_type == SGI_O200 ||
		    sys_config.system_type == SGI_O300) {
			has_ethernet = has_ps2 = has_serial = 1;
			dual_irq = 1;
			/*
			 * XXX On IP35 class machines, there are no
			 * XXX Number-In-a-Can chips to tell us the
			 * XXX Ethernet address, we need to query
			 * XXX the L1 controller.
			 */
		} else {
unknown:
			/*
			 * Well, we don't really know what kind of device
			 * we are.  We should probe various registers
			 * to figure out, but for now we'll just
			 * chicken out.
			 */
			printf("unknown flavour\n");
			return;
		}
	}

	/*
	 * IOC3 is not a real PCI device - it's a poor wrapper over a set
	 * of convenience chips. And when it is in full-blown configuration,
	 * it actually needs to use two interrupts, one for the superio
	 * chip, and the other for the Ethernet chip.
	 *
	 * Since our pci layer doesn't handle this, we have to compute
	 * the superio interrupt cookie ourselves, with the help of the
	 * pci bridge driver.
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

	if (pci_intr_map(pa, &ih1) != 0) {
		printf("failed to map interrupt!\n");
		goto unmap;
	}

	/*
	 * The second vector source seems to be the first unused PCI
	 * slot.
	 */
	if (dual_irq) {
		for (dev = 0;
		    dev < pci_bus_maxdevs(pa->pa_pc, pa->pa_bus); dev++) {
			pcitag_t tag;
			int line, rc;

			if (dev == pa->pa_device)
				continue;

			tag = pci_make_tag(pa->pa_pc, pa->pa_bus, dev, 0);
			if (pci_conf_read(pa->pa_pc, tag, PCI_ID_REG) !=
			    0xffffffff)
				continue;	/* slot in use... */

			line = pa->pa_intrline;
			pa->pa_intrline = dev;
			rc = pci_intr_map(pa, &ih2);
			pa->pa_intrline = line;

			if (rc != 0) {
				printf(": failed to map superio interrupt!\n");
				goto unmap;
			}

			goto establish;
		}

		/*
		 * There are no empty slots, thus we can't steal an
		 * interrupt. I don't know how IOC3 behaves in this
		 * situation, but it's probably safe to revert to
		 * a shared, single interrupt.
		 */
		shared_handler = 1;
		dual_irq = 0;
	}

	if (dual_irq) {
establish:
		/*
		 * Register the second (superio) interrupt.
		 */

		sc->sc_ih2 = pci_intr_establish(sc->sc_pc, ih2, IPL_TTY,
		    ioc_intr_superio, sc, self->dv_xname);
		if (sc->sc_ih2 == NULL) {
			printf("failed to establish superio interrupt at %s\n",
			    pci_intr_string(sc->sc_pc, ih2));
			goto unmap;
		}

		printf("superio %s", pci_intr_string(sc->sc_pc, ih2));
	}

	/*
	 * Register the main (Ethernet if available, superio otherwise)
	 * interrupt.
	 */

	sc->sc_ih1 = pci_intr_establish(sc->sc_pc, ih1, IPL_NET,
	    shared_handler ? ioc_intr_shared : ioc_intr_ethernet,
	    sc, self->dv_xname);
	if (sc->sc_ih1 == NULL) {
		printf("\n%s: failed to establish %sinterrupt!\n",
		    self->dv_xname, dual_irq ? "ethernet " : "");
		goto unregister;
	}
	printf("%s%s\n", dual_irq ? ", ethernet " : "",
	    pci_intr_string(sc->sc_pc, ih1));

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
	 * Attach other sub-devices.
	 */

	if (has_serial) {
		ioc_attach_child(self, "com", IOC3_UARTA_BASE, IOCDEV_SERIAL_A);
		ioc_attach_child(self, "com", IOC3_UARTB_BASE, IOCDEV_SERIAL_B);
	}
	if (has_ps2)
		ioc_attach_child(self, "iockbc", 0, IOCDEV_KEYBOARD);
	if (has_ethernet)
		ioc_attach_child(self, "iec", IOC3_EF_BASE, IOCDEV_EF);
	/* XXX what about the parallel port? */
	ioc_attach_child(self, "dsrtc", 0, IOCDEV_RTC);

	return;

unregister2:
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih1);
unregister:
	if (dual_irq)
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih2);
unmap:
	bus_space_unmap(memt, memh, memsize);
}

void
ioc_attach_child(struct device *ioc, const char *name, bus_addr_t base, int dev)
{
	struct ioc_softc *sc = (struct ioc_softc *)ioc;
	struct ioc_attach_args iaa;

	iaa.iaa_name = name;
	iaa.iaa_memt = sc->sc_memt;
	iaa.iaa_memh = sc->sc_memh;
	iaa.iaa_dmat = sc->sc_dmat;
	iaa.iaa_base = base;
	iaa.iaa_dev = dev;

	if (sc->sc_owmac != NULL)
		memcpy(iaa.iaa_enaddr, sc->sc_owmac->sc_enaddr, 6);
	else {
		/*
		 * XXX On IP35, there is no Number-In-a-Can attached to
		 * XXX the onboard IOC3; instead, the Ethernet address
		 * XXX is stored in the machine eeprom and can be
		 * XXX queried by sending the appropriate L1 command
		 * XXX to the L1 UART. This L1 code is not written yet.
		 */
		bzero(iaa.iaa_enaddr, 6);
	}

	config_found_sm(ioc, &iaa, ioc_print, ioc_search_mundane);
}

int
ioc_search_mundane(struct device *parent, void *vcf, void *args)
{
	struct cfdata *cf = vcf;
	struct ioc_attach_args *iaa = (struct ioc_attach_args *)args;

	if (strcmp(cf->cf_driver->cd_name, iaa->iaa_name) != 0)
		return 0;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != (int)iaa->iaa_base)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, cf, iaa);
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
	extern struct cfdriver owserial_cd;
	struct owserial_softc *s;

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

	/*
	 * Find the first owserial child of the onewire bus not
	 * reporting power supply information, and keep a pointer
	 * to it.  This is a bit overkill since we do not need to
	 * keep the pointer after attach, but it makes that kind
	 * of code contained in the same place.
	 */
	if (owdev != NULL) {
		TAILQ_FOREACH(dev, &alldevs, dv_list)
			if (dev->dv_parent == owdev &&
			    dev->dv_cfdata->cf_driver == &owserial_cd) {
				s = (struct owserial_softc *)dev;
				if (strncmp(s->sc_name, "PWR", 3) == 0)
					continue;
				sc->sc_owserial = s;
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
			(void)ioc_intr_dispatch(sc, dev);

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
	struct ioc_softc *sc = (struct ioc_softc *)v;

	/* This interrupt source is not shared between several devices. */
	return ioc_intr_dispatch(sc, IOCDEV_EF);
}

int
ioc_intr_shared(void *v)
{
	return ioc_intr_superio(v) | ioc_intr_ethernet(v);
}

int
ioc_intr_dispatch(struct ioc_softc *sc, int dev)
{
	struct ioc_intr *ii;
	int rc = 0;

	/* Call registered interrupt function. */
	if ((ii = sc->sc_intr[dev]) != NULL && ii->ii_func != NULL) {
		rc = (*ii->ii_func)(ii->ii_arg);
		if (rc != 0)
			ii->ii_count.ec_count++;
	}

	return rc;
}
