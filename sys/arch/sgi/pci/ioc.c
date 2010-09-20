/*	$OpenBSD: ioc.c,v 1.36 2010/09/20 06:33:47 matthew Exp $	*/

/*
 * Copyright (c) 2008 Joel Sing.
 * Copyright (c) 2008, 2009, 2010 Miodrag Vallat.
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
 * IOC3 device driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <mips64/archtype.h>
#include <machine/autoconf.h>
#include <machine/bus.h>

#ifdef TGT_ORIGIN
#include <sgi/sgi/ip27.h>
#include <sgi/sgi/l1.h>
#endif

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
	pcitag_t		 sc_tag;

	void			*sc_ih_enet;	/* Ethernet interrupt */
	void			*sc_ih_superio;	/* SuperIO interrupt */
	struct ioc_intr		*sc_intr[IOC_NDEVS];

	struct onewire_bus	 sc_bus;

	struct owmac_softc	*sc_owmac;
	struct owserial_softc	*sc_owserial;

	int			 sc_attach_flags;
};

struct cfattach ioc_ca = {
	sizeof(struct ioc_softc), ioc_match, ioc_attach,
};

struct cfdriver ioc_cd = {
	NULL, "ioc", DV_DULL,
};

void	ioc_attach_child(struct ioc_softc *, const char *, bus_addr_t, int);
int	ioc_search_onewire(struct device *, void *, void *);
int	ioc_search_mundane(struct device *, void *, void *);
int	ioc_print(void *, const char *);

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

#ifdef TGT_ORIGIN
/*
 * A mask of nodes on which an ioc driver has attached.
 * We use this on IP35 systems, to prevent attaching a pci IOC3 card which NIC
 * has failed, as the onboard IOC3.
 * XXX This obviously will not work in N mode... but then IP35 are supposed to
 * XXX always run in M mode.
 */
static	uint64_t ioc_nodemask = 0;
#endif

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

	/* no base for onewire, and don't display it for rtc */
	if ((int)iaa->iaa_base > 0 && (int)iaa->iaa_base < IOC3_BYTEBUS_0)
		printf(" base 0x%x", iaa->iaa_base);

	return (UNCONF);
}

void
ioc_attach(struct device *parent, struct device *self, void *aux)
{
	struct ioc_softc *sc = (struct ioc_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih_enet, ih_superio;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_size_t memsize;
	pcireg_t data;
	int has_superio, has_enet, is_obio;
	int subdevice_mask;
	bus_addr_t rtcbase;

	if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM, 0,
	    &memt, &memh, NULL, &memsize, 0)) {
		printf(": can't map mem space\n");
		return;
	}

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
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
		goto unmap;
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

	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_GPCR_S,
	    IOC3_GPCR_MLAN);
	(void)bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC3_GPCR_S);
	config_search(ioc_search_onewire, self, aux);

	/*
	 * Now figure out what our configuration is.
	 */

	has_superio = has_enet = 0;
	is_obio = 0;
	subdevice_mask = 0;
	if (sc->sc_owserial != NULL) {
		if (strncmp(sc->sc_owserial->sc_product, "030-0873-", 9) == 0) {
			/*
			 * MENET board; these attach as four ioc devices
			 * behind an xbridge. However the fourth one lacks
			 * the superio chip.
			 */
			subdevice_mask = (1 << IOCDEV_EF);
			has_enet = 1;
			if (pa->pa_device != 3) {
				subdevice_mask = (1 << IOCDEV_SERIAL_A) |
				    (1 << IOCDEV_SERIAL_B);
				has_superio = 1;
			}
		} else
		if (strncmp(sc->sc_owserial->sc_product, "030-0891-", 9) == 0) {
			/* IP30 on-board IOC3 */
			subdevice_mask = (1 << IOCDEV_SERIAL_A) |
			    (1 << IOCDEV_SERIAL_B) | (1 << IOCDEV_LPT) |
			    (1 << IOCDEV_KBC) | (1 << IOCDEV_RTC) |
			    (1 << IOCDEV_EF);
			rtcbase = IOC3_BYTEBUS_1;
			has_superio = has_enet = 1;
			is_obio = 1;
		} else
		if (strncmp(sc->sc_owserial->sc_product, "030-1155-", 9) == 0) {
			/* CADDuo board */
			subdevice_mask = (1 << IOCDEV_KBC) | (1 << IOCDEV_EF);
			has_superio = has_enet = 1;
		} else
		if (strncmp(sc->sc_owserial->sc_product, "030-1657-", 9) == 0 ||
		    strncmp(sc->sc_owserial->sc_product, "030-1664-", 9) == 0) {
			/* PCI_SIO_UFC dual serial board */
			subdevice_mask = (1 << IOCDEV_SERIAL_A) |
			    (1 << IOCDEV_SERIAL_B);
			has_superio = 1;
		} else
			goto unknown;
	} else {
#ifdef TGT_ORIGIN
		/*
		 * If no owserial device has been found, then it is
		 * very likely that we are the on-board IOC3 found
		 * on IP27 and IP35 systems, unless we have already
		 * found an on-board IOC3 on this node.
		 *
		 * Origin 2000 (real IP27) systems are a real annoyance,
		 * because they actually have two IOC3 on their BASEIO
		 * board, with the various devices split accross them
		 * (two IOC3 chips are needed to provide the four serial
		 * ports). We can rely upon the PCI device numbers (2 and 6)
		 * to tell onboard IOC3 from PCI IOC3 devices.
		 */
		switch (sys_config.system_type) {
		case SGI_IP27:
			switch (sys_config.system_subtype) {
			case IP27_O2K:
				if (pci_get_widget(sc->sc_pc) ==
				    IP27_O2K_BRIDGE_WIDGET)
					switch (pa->pa_device) {
					case IP27_IOC_SLOTNO:
						subdevice_mask =
						    (1 << IOCDEV_SERIAL_A) |
						    (1 << IOCDEV_SERIAL_B) |
						    (1 << IOCDEV_RTC) |
						    (1 << IOCDEV_EF);
						break;
					case IP27_IOC2_SLOTNO:
						subdevice_mask =
						    (1 << IOCDEV_SERIAL_A) |
						    (1 << IOCDEV_SERIAL_B) |
						    (1 << IOCDEV_LPT) |
#if 0 /* not worth doing */
						    (1 << IOCDEV_RTC) |
#endif
						    (1 << IOCDEV_KBC);
						break;
					default:
						break;
					}
				break;
			case IP27_O200:
				if (pci_get_widget(sc->sc_pc) ==
				    IP27_O200_BRIDGE_WIDGET)
					switch (pa->pa_device) {
					case IP27_IOC_SLOTNO:
						subdevice_mask =
						    (1 << IOCDEV_SERIAL_A) |
						    (1 << IOCDEV_SERIAL_B) |
						    (1 << IOCDEV_LPT) |
						    (1 << IOCDEV_KBC) |
						    (1 << IOCDEV_RTC) |
						    (1 << IOCDEV_EF);
						break;
					default:
						break;
					}
				break;
			default:
				break;
			}
			break;
		case SGI_IP35:
			if (!ISSET(ioc_nodemask, 1UL << currentnasid)) {
				SET(ioc_nodemask, 1UL << currentnasid);

				switch (sys_config.system_subtype) {
				/*
				 * Origin 300 onboard IOC3 do not have PS/2
				 * ports; since they can only be connected to
				 * other 300 or 350 bricks (the latter using
				 * IOC4 devices), it is safe to do this
				 * regardless of the current nasid.
				 * XXX What about Onyx 300 though???
				 */
				case IP35_O300:
					subdevice_mask =
					    (1 << IOCDEV_SERIAL_A) |
					    (1 << IOCDEV_SERIAL_B) |
					    (1 << IOCDEV_LPT) |
					    (1 << IOCDEV_RTC) |
					    (1 << IOCDEV_EF);
					break;
				/*
				 * Origin 3000 I-Bricks have only one serial
				 * port, and no keyboard or parallel ports.
				 */
				case IP35_CBRICK:
					subdevice_mask =
					    (1 << IOCDEV_SERIAL_A) |
					    (1 << IOCDEV_RTC) |
					    (1 << IOCDEV_EF);
					break;
				default:
					subdevice_mask =
					    (1 << IOCDEV_SERIAL_A) |
					    (1 << IOCDEV_SERIAL_B) |
					    (1 << IOCDEV_LPT) |
					    (1 << IOCDEV_KBC) |
					    (1 << IOCDEV_RTC) |
					    (1 << IOCDEV_EF);
					break;
				}
			}
			break;
		default:
			break;
		}

		if (subdevice_mask != 0) {
			rtcbase = IOC3_BYTEBUS_0;
			has_superio = 1;
			if (ISSET(subdevice_mask, 1 << IOCDEV_EF))
				has_enet = 1;
			is_obio = 1;
		} else
#endif	/* TGT_ORIGIN */
		{
unknown:
			/*
			 * Well, we don't really know what kind of device
			 * we are.  We should probe various registers
			 * to figure out, but for now we'll just
			 * chicken out.
			 */
			printf("%s: unknown flavour\n", self->dv_xname);
			return;
		}
	}

	/*
	 * Acknowledge all pending interrupts, and disable them.
	 * Be careful not all registers may be wired depending on what
	 * devices are actually present.
	 */

	if (has_superio) {
		bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IEC, ~0x0);
		bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IES, 0x0);
		bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IR,
		    bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IR));
	}
	if (has_enet) {
		bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_ENET_IER, 0);
	}

	/*
	 * IOC3 is not a real PCI device - it's a poor wrapper over a set
	 * of convenience chips. And when it is in full-blown configuration,
	 * it actually needs to use two interrupts, one for the superio
	 * chip, and the other for the Ethernet chip.
	 *
	 * This would not be a problem if the device advertized itself
	 * as a multifunction device. But it doesn't...
	 *
	 * Fortunately, the interrupt used are simply interrupt pins A
	 * and B; so with the help of the PCI bridge driver, we can
	 * register the two interrupts and almost pretend things are
	 * as normal as they could be.
	 */

	if (has_enet) {
		pa->pa_intrpin = PCI_INTERRUPT_PIN_A;
		if (pci_intr_map(pa, &ih_enet) != 0) {
			printf("%s: failed to map ethernet interrupt\n",
			    self->dv_xname);
			goto unmap;
		}
	}

	if (has_superio) {
		if (has_enet)
			pa->pa_intrpin =
			    is_obio ? PCI_INTERRUPT_PIN_D : PCI_INTERRUPT_PIN_B;
		else
			pa->pa_intrpin = PCI_INTERRUPT_PIN_A;

		if (pci_intr_map(pa, &ih_superio) != 0) {
			printf("%s: failed to map superio interrupt\n",
			    self->dv_xname);
			goto unmap;
		}
	}

	if (has_enet) {
		sc->sc_ih_enet = pci_intr_establish(sc->sc_pc, ih_enet,
		    IPL_NET, ioc_intr_ethernet, sc, self->dv_xname);
		if (sc->sc_ih_enet == NULL) {
			printf("%s: failed to establish ethernet interrupt "
			    "at %s\n", self->dv_xname,
			    pci_intr_string(sc->sc_pc, ih_enet));
			goto unmap;
		}
	}

	if (has_superio) {
		sc->sc_ih_superio = pci_intr_establish(sc->sc_pc, ih_superio,
		    IPL_TTY, ioc_intr_superio, sc, self->dv_xname);
		if (sc->sc_ih_superio == NULL) {
			printf("%s: failed to establish superio interrupt "
			    "at %s\n", self->dv_xname,
			    pci_intr_string(sc->sc_pc, ih_superio));
			goto unregister;
		}
	}

	if (has_enet)
		printf("%s: ethernet %s\n",
		    self->dv_xname, pci_intr_string(sc->sc_pc, ih_enet));
	if (has_superio)
		printf("%s: superio %s\n",
		    self->dv_xname, pci_intr_string(sc->sc_pc, ih_superio));

	/*
	 * Attach other sub-devices.
	 */

	sc->sc_attach_flags = is_obio ? IOC_FLAGS_OBIO : 0;

	if (ISSET(subdevice_mask, 1 << IOCDEV_SERIAL_A)) {
		/*
		 * Put serial ports in passthrough mode,
		 * to use the MI com(4) 16550 support.
		 */
		bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_UARTA_SSCR, 0);
		bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_UARTB_SSCR, 0);

		bus_space_write_4(sc->sc_memt, sc->sc_memh,
		    IOC3_UARTA_SHADOW, 0);
		bus_space_write_4(sc->sc_memt, sc->sc_memh,
		    IOC3_UARTB_SHADOW, 0);

		ioc_attach_child(sc, "com", IOC3_UARTA_BASE, IOCDEV_SERIAL_A);
		if (ISSET(subdevice_mask, 1 << IOCDEV_SERIAL_B))
			ioc_attach_child(sc, "com", IOC3_UARTB_BASE,
			    IOCDEV_SERIAL_B);
	}
	if (ISSET(subdevice_mask, 1 << IOCDEV_KBC))
		ioc_attach_child(sc, "iockbc", 0, IOCDEV_KBC);
	if (ISSET(subdevice_mask, 1 << IOCDEV_EF))
		ioc_attach_child(sc, "iec", 0, IOCDEV_EF);
	if (ISSET(subdevice_mask, 1 << IOCDEV_LPT))
		ioc_attach_child(sc, "lpt", 0, IOCDEV_LPT);
	if (ISSET(subdevice_mask, 1 << IOCDEV_RTC))
		ioc_attach_child(sc, "dsrtc", rtcbase, IOCDEV_RTC);

	return;

unregister:
	if (has_enet)
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih_enet);
unmap:
	bus_space_unmap(memt, memh, memsize);
}

void
ioc_attach_child(struct ioc_softc *sc, const char *name, bus_addr_t base,
    int dev)
{
	struct ioc_attach_args iaa;

	memset(&iaa, 0, sizeof iaa);

	iaa.iaa_name = name;
	pci_get_device_location(sc->sc_pc, sc->sc_tag, &iaa.iaa_location);
	iaa.iaa_memt = sc->sc_memt;
	iaa.iaa_memh = sc->sc_memh;
	iaa.iaa_dmat = sc->sc_dmat;
	iaa.iaa_base = base;
	iaa.iaa_dev = dev;
	iaa.iaa_flags = sc->sc_attach_flags;

	if (dev == IOCDEV_EF) {
		if (sc->sc_owmac != NULL)
			memcpy(iaa.iaa_enaddr, sc->sc_owmac->sc_enaddr, 6);
		else {
#ifdef TGT_ORIGIN
			/*
			 * On IP35 class machines, there are no
			 * Number-In-a-Can attached to the onboard
			 * IOC3; instead, the Ethernet address is
			 * stored in the Brick EEPROM, and can be
			 * retrieved with an L1 controller query.
			 */
			if (sys_config.system_type != SGI_IP35 ||
			    l1_get_brick_ethernet_address(currentnasid,
			      iaa.iaa_enaddr) != 0)
#endif
				memset(iaa.iaa_enaddr, 0xff, 6);
		}
	}

	config_found_sm(&sc->sc_dev, &iaa, ioc_print, ioc_search_mundane);
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
static const uint32_t ioc_intrbits[IOC_NDEVS] = {
	IOC3_IRQ_UARTA,
	IOC3_IRQ_UARTB,
	IOC3_IRQ_LPT,
	IOC3_IRQ_KBC,
	0,	/* RTC */
	0	/* Ethernet, handled differently */
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

	evcount_attach(&ii->ii_count, name, &ii->ii_level);
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
	uint32_t pending, mask;
	int dev;

	pending = bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IR) &
	    bus_space_read_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IES);

	if (pending == 0)
		return 0;

	/* Disable pending interrupts */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, IOC3_SIO_IEC, pending);

	for (dev = 0; dev < IOC_NDEVS - 1 /* skip Ethernet */; dev++) {
		mask = pending & ioc_intrbits[dev];
		if (mask != 0) {
			(void)ioc_intr_dispatch(sc, dev);

			/* Ack, then reenable, pending interrupts */
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    IOC3_SIO_IR, mask);
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    IOC3_SIO_IES, mask);
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
