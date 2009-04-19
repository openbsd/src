/*	$OpenBSD: xbridge.c,v 1.12 2009/04/19 18:37:31 miod Exp $	*/

/*
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
 * XBow Bridge Widget driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <mips64/archtype.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>

#include <sgi/xbow/xbridgereg.h>

#include <sgi/sgi/ip30.h>

int	xbridge_match(struct device *, void *, void *);
void	xbridge_attach(struct device *, struct device *, void *);
int	xbridge_print(void *, const char *);

struct xbridge_intr;

struct xbridge_softc {
	struct device	sc_dev;
	int		sc_rev;
	int		sc_widget;
	struct mips_pci_chipset sc_pc;

	struct mips_bus_space *sc_mem_bus_space;
	struct mips_bus_space *sc_io_bus_space;
	struct machine_bus_dma_tag sc_dmatag;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_regh;

	int		sc_intrbit[BRIDGE_NINTRS];
	struct xbridge_intr	*sc_intr[BRIDGE_NINTRS];
};

const struct cfattach xbridge_ca = {
	sizeof(struct xbridge_softc), xbridge_match, xbridge_attach,
};

struct cfdriver xbridge_cd = {
	NULL, "xbridge", DV_DULL,
};

void	xbridge_attach_hook(struct device *, struct device *,
				struct pcibus_attach_args *);
pcitag_t xbridge_make_tag(void *, int, int, int);
void	xbridge_decompose_tag(void *, pcitag_t, int *, int *, int *);
int	xbridge_bus_maxdevs(void *, int);
pcireg_t xbridge_conf_read(void *, pcitag_t, int);
void	xbridge_conf_write(void *, pcitag_t, int, pcireg_t);

int	xbridge_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *xbridge_intr_string(void *, pci_intr_handle_t);
void	*xbridge_intr_establish(void *, pci_intr_handle_t, int,
	    int (*func)(void *), void *, char *);
void	xbridge_intr_disestablish(void *, void *);
int	xbridge_intr_handler(void *);

uint8_t xbridge_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
uint16_t xbridge_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	xbridge_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint8_t);
void	xbridge_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint16_t);
void	xbridge_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	xbridge_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
void	xbridge_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	xbridge_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
void	xbridge_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	xbridge_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
int	xbridge_space_map_short(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);

bus_addr_t xbridge_pa_to_device(paddr_t);
paddr_t	xbridge_device_to_pa(bus_addr_t);

const struct machine_bus_dma_tag xbridge_dma_tag = {
	NULL,			/* _cookie */
	_dmamap_create,
	_dmamap_destroy,
	_dmamap_load,
	_dmamap_load_mbuf,
	_dmamap_load_uio,
	_dmamap_load_raw,
	_dmamap_unload,
	_dmamap_sync,
	_dmamem_alloc,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap,
	xbridge_pa_to_device,
	xbridge_device_to_pa,
	0ULL	/* no mask */
};

int
xbridge_match(struct device *parent, void *match, void *aux)
{
	struct xbow_attach_args *xaa = aux;

	if (xaa->xaa_vendor == XBOW_VENDOR_SGI4 &&
	    xaa->xaa_product == XBOW_PRODUCT_SGI4_BRIDGE)
		return 1;

	if (xaa->xaa_vendor == XBOW_VENDOR_SGI3 &&
	    xaa->xaa_product == XBOW_PRODUCT_SGI3_XBRIDGE)
		return 1;

	return 0;
}

void
xbridge_attach(struct device *parent, struct device *self, void *aux)
{
	struct xbridge_softc *sc = (struct xbridge_softc *)self;
	struct pcibus_attach_args pba;
	struct xbow_attach_args *xaa = aux;
	int i;

	sc->sc_rev = xaa->xaa_revision;
	sc->sc_widget = xaa->xaa_widget;

	printf(" revision %d\n", sc->sc_rev);

	/*
	 * Map Bridge registers.
	 */

	sc->sc_iot = xaa->xaa_short_tag;
	if (bus_space_map(sc->sc_iot, 0, BRIDGE_REGISTERS_SIZE, 0,
	    &sc->sc_regh)) {
		printf("%s: unable to map control registers\n", self->dv_xname);
		return;
	}

	/*
	 * Create bus_space accessors... we inherit them from xbow, but
	 * it is necessary to perform endianness conversion for the
	 * low-order address bits.
	 */

	sc->sc_mem_bus_space = malloc(sizeof (*sc->sc_mem_bus_space),
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_mem_bus_space == NULL)
		goto fail1;
	sc->sc_io_bus_space = malloc(sizeof (*sc->sc_io_bus_space),
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_io_bus_space == NULL)
		goto fail2;

	if (sys_config.system_type == SGI_OCTANE) {
		bcopy(xaa->xaa_long_tag, sc->sc_mem_bus_space,
		    sizeof(*sc->sc_mem_bus_space));
		sc->sc_mem_bus_space->bus_base += BRIDGE_PCI_MEM_SPACE_BASE;

		if (sc->sc_rev >= 4) {
			/* Unrestricted I/O mappings in the large window */
			bcopy(xaa->xaa_long_tag, sc->sc_io_bus_space,
			    sizeof(*sc->sc_io_bus_space));
			sc->sc_io_bus_space->bus_base +=
			    BRIDGE_PCI_IO_SPACE_BASE;
		} else {
			/* Programmable I/O mappings in the small window */
			bcopy(xaa->xaa_short_tag, sc->sc_io_bus_space,
			    sizeof(*sc->sc_io_bus_space));
		}
	} else {
		/* Limited memory mappings in the small window */
		bcopy(xaa->xaa_short_tag, sc->sc_mem_bus_space,
		    sizeof(*sc->sc_mem_bus_space));
		sc->sc_mem_bus_space->bus_private = sc;
		sc->sc_mem_bus_space->_space_map = xbridge_space_map_short;

		/* Limited I/O mappings in the small window */
		bcopy(xaa->xaa_short_tag, sc->sc_io_bus_space,
		    sizeof(*sc->sc_io_bus_space));
		sc->sc_io_bus_space->bus_private = sc;
		sc->sc_io_bus_space->_space_map = xbridge_space_map_short;
	}

	sc->sc_io_bus_space->_space_read_1 = xbridge_read_1;
	sc->sc_io_bus_space->_space_write_1 = xbridge_write_1;
	sc->sc_io_bus_space->_space_read_2 = xbridge_read_2;
	sc->sc_io_bus_space->_space_write_2 = xbridge_write_2;
	sc->sc_io_bus_space->_space_read_raw_2 = xbridge_read_raw_2;
	sc->sc_io_bus_space->_space_write_raw_2 = xbridge_write_raw_2;
	sc->sc_io_bus_space->_space_read_raw_4 = xbridge_read_raw_4;
	sc->sc_io_bus_space->_space_write_raw_4 = xbridge_write_raw_4;
	sc->sc_io_bus_space->_space_read_raw_8 = xbridge_read_raw_8;
	sc->sc_io_bus_space->_space_write_raw_8 = xbridge_write_raw_8;

	sc->sc_mem_bus_space->_space_read_1 = xbridge_read_1;
	sc->sc_mem_bus_space->_space_write_1 = xbridge_write_1;
	sc->sc_mem_bus_space->_space_read_2 = xbridge_read_2;
	sc->sc_mem_bus_space->_space_write_2 = xbridge_write_2;
	sc->sc_mem_bus_space->_space_read_raw_2 = xbridge_read_raw_2;
	sc->sc_mem_bus_space->_space_write_raw_2 = xbridge_write_raw_2;
	sc->sc_mem_bus_space->_space_read_raw_4 = xbridge_read_raw_4;
	sc->sc_mem_bus_space->_space_write_raw_4 = xbridge_write_raw_4;
	sc->sc_mem_bus_space->_space_read_raw_8 = xbridge_read_raw_8;
	sc->sc_mem_bus_space->_space_write_raw_8 = xbridge_write_raw_8;

	/*
	 * Initialize PCI methods.
	 */

	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = xbridge_attach_hook;
	sc->sc_pc.pc_make_tag = xbridge_make_tag;
	sc->sc_pc.pc_decompose_tag = xbridge_decompose_tag;
	sc->sc_pc.pc_bus_maxdevs = xbridge_bus_maxdevs;
	sc->sc_pc.pc_conf_read = xbridge_conf_read;
	sc->sc_pc.pc_conf_write = xbridge_conf_write;
	sc->sc_pc.pc_intr_v = sc;
	sc->sc_pc.pc_intr_map = xbridge_intr_map;
	sc->sc_pc.pc_intr_string = xbridge_intr_string;
	sc->sc_pc.pc_intr_establish = xbridge_intr_establish;
	sc->sc_pc.pc_intr_disestablish = xbridge_intr_disestablish;

	/*
	 * XXX The following magic sequence is supposedly needed for DMA
	 * XXX to work correctly.  I have no idea what it really does.
	 */
	if (sys_config.system_type == SGI_OCTANE) {
		bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_DIR_MAP,
		    (xbow_intr_widget << 20) | (1 << 17));
#if 0
		bus_space_write_4(sc->sc_iot, sc->sc_regh, 0x284,
		    0x99889988 | 0x44440000);
		bus_space_write_4(sc->sc_iot, sc->sc_regh, 0x28c,
		    0x99889988 | 0x44440000);
#endif
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_DIR_MAP,
		    xbow_intr_widget << 20);
#if 0
		bus_space_write_4(sc->sc_iot, sc->sc_regh, 0x284,
		    0xba98ba98 | 0x44440000);
		bus_space_write_4(sc->sc_iot, sc->sc_regh, 0x28c,
		    0xba98ba98 | 0x44440000);
#endif
	}

	(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);

	/*
	 * Setup interrupt handling.
	 */

	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_IER, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_MODE, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_DEV, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_regh, WIDGET_INTDEST_ADDR_UPPER,
	    (xbow_intr_widget_register  >> 32) | (xbow_intr_widget << 16));
	bus_space_write_4(sc->sc_iot, sc->sc_regh, WIDGET_INTDEST_ADDR_LOWER,
	    (uint32_t)xbow_intr_widget_register);

	(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);

	for (i = 0; i < BRIDGE_NINTRS; i++)
		sc->sc_intrbit[i] = -1;

	/*
	 * Attach children.
	 */

	bcopy(&xbridge_dma_tag, &sc->sc_dmatag, sizeof(xbridge_dma_tag));
	if (sys_config.system_type == SGI_OCTANE) {
		/*
		 * Make sure we do not risk crossing the direct mapping
		 * window.
		 */
		sc->sc_dmatag._dma_mask = BRIDGE_DMA_DIRECT_LENGTH - 1;
	}

	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = sc->sc_io_bus_space;
	pba.pba_memt = sc->sc_mem_bus_space;
	pba.pba_dmat = &sc->sc_dmatag;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0;

	config_found(self, &pba, xbridge_print);
	return;

fail2:
	free(sc->sc_mem_bus_space, M_DEVBUF);
fail1:
	printf("%s: not enough memory to build access structures\n",
	    self->dv_xname);
	return;
}

int
xbridge_print(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);

	return UNCONF;
}

void
xbridge_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

pcitag_t
xbridge_make_tag(void *cookie, int bus, int dev, int func)
{
	return (bus << 16) | (dev << 11) | (func << 8);
}

void
xbridge_decompose_tag(void *cookie, pcitag_t tag, int *busp, int *devp,
    int *funcp)
{
	if (busp != NULL)
		*busp = (tag >> 16) & 0x7;
	if (devp != NULL)
		*devp = (tag >> 11) & 0x1f;
	if (funcp != NULL)
		*funcp = (tag >> 8) & 0x7;
}

int
xbridge_bus_maxdevs(void *cookie, int busno)
{
	return BRIDGE_NSLOTS;
}

pcireg_t
xbridge_conf_read(void *cookie, pcitag_t tag, int offset)
{
	struct xbridge_softc *sc = cookie;
	pcireg_t id, data;
	int bus, dev, fn;
	paddr_t pa;
	int s;

	/* XXX should actually disable interrupts? */
	s = splhigh();

	xbridge_decompose_tag(cookie, tag, &bus, &dev, &fn);
	if (bus != 0) {
		bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_PCI_CFG,
		    (bus << 16) | (dev << 11));
		pa = sc->sc_regh + BRIDGE_PCI_CFG1_SPACE;
	} else
		pa = sc->sc_regh + BRIDGE_PCI_CFG_SPACE + (dev << 12);

	/*
	 * IOC3 devices only implement a subset of the PCI configuration
	 * registers.
	 * Depending on which particular model we encounter, things may
	 * seem to work, or write access to nonexisting registers would
	 * completely freeze the machine.
	 *
	 * We thus check for the device type here, and handle the non
	 * existing registers ourselves.
	 */

	if (guarded_read_4(pa + PCI_ID_REG, &id) != 0) {
		splx(s);
		return 0xffffffff;
	}

	if (id == PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3)) {
		switch (offset) {
		case PCI_ID_REG:
		case PCI_COMMAND_STATUS_REG:
		case PCI_CLASS_REG:
		case PCI_BHLC_REG:
		case PCI_MAPREG_START:
			/* These registers are implemented. Go ahead. */
			id = 0;
			break;
		case PCI_INTERRUPT_REG:
			/* This register is not implemented. Fake it. */
			data = PCI_INTERRUPT_PIN_A << PCI_INTERRUPT_PIN_SHIFT;
			break;
		default:
			/* These registers are not implemented. */
			data = 0;
			break;
		}
	} else
		id = 0;

	if (id == 0) {
		pa += (fn << 8) + offset;
		if (guarded_read_4(pa, &data) != 0)
			data = 0xffffffff;
	}

	splx(s);
	return(data);
}

void
xbridge_conf_write(void *cookie, pcitag_t tag, int offset, pcireg_t data)
{
	struct xbridge_softc *sc = cookie;
	pcireg_t id;
	int bus, dev, fn;
	paddr_t pa;
	int s;

	/* XXX should actually disable interrupts? */
	s = splhigh();

	xbridge_decompose_tag(cookie, tag, &bus, &dev, &fn);
	if (bus != 0) {
		bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_PCI_CFG,
		    (bus << 16) | (dev << 11));
		pa = sc->sc_regh + BRIDGE_PCI_CFG1_SPACE;
	} else
		pa = sc->sc_regh + BRIDGE_PCI_CFG_SPACE + (dev << 12);

	/*
	 * IOC3 devices only implement a subset of the PCI configuration
	 * registers.
	 * Depending on which particular model we encounter, things may
	 * seem to work, or write access to nonexisting registers would
	 * completely freeze the machine.
	 *
	 * We thus check for the device type here, and handle the non
	 * existing registers ourselves.
	 */

	if (guarded_read_4(pa + PCI_ID_REG, &id) != 0) {
		splx(s);
		return;
	}

	if (id == PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3)) {
		switch (offset) {
		case PCI_COMMAND_STATUS_REG:
			/*
			 * Some IOC models do not support having this bit
			 * cleared (which is what pci_mapreg_probe() will
			 * do), so we set it unconditionnaly.
			 */
			data |= PCI_COMMAND_MEM_ENABLE;
			/* FALLTHROUGH */
		case PCI_ID_REG:
		case PCI_CLASS_REG:
		case PCI_BHLC_REG:
		case PCI_MAPREG_START:
			/* These registers are implemented. Go ahead. */
			id = 0;
			break;
		default:
			/* These registers are not implemented. */
			break;
		}
	} else
		id = 0;

	if (id == 0) {
		pa += (fn << 8) + offset;
		guarded_write_4(pa, data);
	}

	splx(s);
}

/*
 * Interrupt handling.
 * We map each slot to its own interrupt bit, which will in turn be routed to
 * the Heart or Hub widget in charge of interrupt processing.
 */

struct xbridge_intr {
	struct	xbridge_softc	*xi_bridge;
	int	xi_intrsrc;

	int	(*xi_func)(void *);
	void	*xi_arg;

	struct evcount	xi_count;
	int	 xi_level;
};

int
xbridge_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int bus, device, intr;

	*ihp = -1;

	if (pa->pa_intrpin == 0) {
		/* No IRQ used. */
		return 1;
	}

#ifdef DIAGNOSTIC
	if (pa->pa_intrpin > 4) {
		printf("%s: bad interrupt pin %d\n", __func__, pa->pa_intrpin);
		return 1;
	}
#endif

	xbridge_decompose_tag(pa->pa_pc, pa->pa_tag, &bus, &device, NULL);
	if (pa->pa_intrpin & 1)
		intr = device;
	else
		intr = device ^ 4;

	*ihp = intr;

	return 0;
}

const char *
xbridge_intr_string(void *cookie, pci_intr_handle_t ih)
{
	static char str[16];

	snprintf(str, sizeof(str), "irq %d", ih);
	return(str);
}

void *
xbridge_intr_establish(void *cookie, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct xbridge_softc *sc = cookie;
	struct xbridge_intr *xi;
	uint32_t int_addr;
	int intrbit = ih & 0x07;
	int intrsrc;
	int16_t nasid = 0;	/* XXX */

	if (sc->sc_intr[intrbit] != NULL) {
		printf("%s: nested interrupts are not supported\n", __func__);
		return NULL;
	}

	xi = (struct xbridge_intr *)malloc(sizeof(*xi), M_DEVBUF, M_NOWAIT);
	if (xi == NULL)
		return NULL;

	/*
	 * Register the interrupt at the Heart or Hub level if it's the
	 * first time we're using this interrupt source.
	 */
	if ((intrsrc = sc->sc_intrbit[intrbit]) == -1) {
		if (xbow_intr_register(sc->sc_widget, level, &intrsrc) != 0)
			return NULL;
	
		/*
		 * We can afford registering this interrupt at `level'
		 * IPL since we do not support nested interrupt on a
		 * given source, yet.
		 */
		if (xbow_intr_establish(xbridge_intr_handler, xi, intrsrc,
		    level, sc->sc_dev.dv_xname)) {
			printf("%s: unable to register interrupt handler, "
			    "did xheart or xhub attach?\n",
			    sc->sc_dev.dv_xname);
			return NULL;
		}

		sc->sc_intrbit[intrbit] = intrsrc;
	}

	xi->xi_bridge = sc;
	xi->xi_intrsrc = intrsrc;
	xi->xi_func = func;
	xi->xi_arg = arg;
	xi->xi_level = level;
	evcount_attach(&xi->xi_count, name, &xi->xi_level, &evcount_intr);
	sc->sc_intr[intrbit] = xi;

	switch (sys_config.system_type) {
	case SGI_OCTANE:
		int_addr = intrsrc;
		break;
	default:
	case SGI_O200:
	case SGI_O300:
		int_addr = 0x20000 | intrsrc | (nasid << 8);
		break;
	}

	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_ADDR(intrbit),
	    int_addr);
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_IER,
	    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_IER) |
	    (1 << intrbit));
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_MODE,
	    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_MODE) |
	    (1 << intrbit));
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_DEV,
	    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_DEV) |
	    (intrbit << (intrbit * 3)));
	(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);

	return (void *)((uint64_t)ih | 8);	/* XXX don't return zero */
}

void
xbridge_intr_disestablish(void *cookie, void *ih)
{
	struct xbridge_softc *sc = cookie;
	struct xbridge_intr *xi;
	int intrbit = (uint64_t)ih & 0x07;

	/* should not happen */
	if ((xi = sc->sc_intr[intrbit]) == NULL)
		return;

	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_ADDR(intrbit), 0);
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_IER,
	    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_IER) &
	    ~(1 << intrbit));
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_MODE,
	    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_MODE) &
	    ~(1 << intrbit));
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_DEV,
	    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_DEV) &
	    ~(7 << (intrbit * 3)));
	(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);

	evcount_detach(&xi->xi_count);

	xbow_intr_disestablish(xi->xi_intrsrc);

	sc->sc_intr[intrbit] = NULL;
	free(xi, M_DEVBUF);
}

int
xbridge_intr_handler(void *v)
{
	struct xbridge_intr *xi = v;
	struct xbridge_softc *sc = xi->xi_bridge;
	int rc;

	if (xi == NULL) {
		printf("%s: spurious interrupt on source %d\n",
		    sc->sc_dev.dv_xname, xi->xi_intrsrc);
		return 0;
	}

	if ((rc = (*xi->xi_func)(xi->xi_arg)) != 0)
		xi->xi_count.ec_count++;

	return rc;
}

/*
 * bus_space helpers
 */

uint8_t
xbridge_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint8_t *)((h + o) ^ 3);
}

uint16_t
xbridge_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint16_t *)((h + o) ^ 2);
}

void
xbridge_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t v)
{
	*(volatile uint8_t *)((h + o) ^ 3) = v;
}

void
xbridge_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t v)
{
	*(volatile uint16_t *)((h + o) ^ 2) = v;
}

void
xbridge_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)((h + o) ^ 2);
	len >>= 1;
	while (len-- != 0) {
		*(uint16_t *)buf = *addr;
		buf += 2;
	}
}

void
xbridge_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)((h + o) ^ 2);
	len >>= 1;
	while (len-- != 0) {
		*addr = *(uint16_t *)buf;
		buf += 2;
	}
}

void
xbridge_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*(uint32_t *)buf = *addr;
		buf += 4;
	}
}

void
xbridge_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*addr = *(uint32_t *)buf;
		buf += 4;
	}
}

void
xbridge_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*(uint64_t *)buf = *addr;
		buf += 8;
	}
}

void
xbridge_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*addr = *(uint64_t *)buf;
		buf += 8;
	}
}

/*
 * On IP27, we can not use the default xbow space_map_short because
 * of the games we play with bus addresses.
 */
int
xbridge_space_map_short(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int cacheable, bus_space_handle_t *bshp)
{
	struct xbridge_softc *sc = (struct xbridge_softc *)t->bus_private;
	bus_addr_t bpa;

	bpa = t->bus_base - (sc->sc_widget << 24) + offs;

	/* check that this neither underflows nor overflows the window */
	if (((bpa + size - 1) >> 24) != (t->bus_base >> 24) ||
	    (bpa >> 24) != (t->bus_base >> 24))
		return (EINVAL);

	*bshp = bpa;
	return 0;
}

/*
 * bus_dma helpers
 */

bus_addr_t
xbridge_pa_to_device(paddr_t pa)
{
	/*
	 * On the Octane, we try to use the direct DMA window whenever
	 * possible; this allows hardware limited to 32 bit DMA addresses
	 * to work.
	 */

	if (sys_config.system_type == SGI_OCTANE) {
		pa -= IP30_MEMORY_BASE;
		if (pa < BRIDGE_DMA_DIRECT_LENGTH)
			return pa + BRIDGE_DMA_DIRECT_BASE;
	}

	pa += ((uint64_t)xbow_intr_widget << 60) | (1UL << 56);

	return pa;
}

paddr_t
xbridge_device_to_pa(bus_addr_t addr)
{
	paddr_t pa;

	pa = addr & ((1UL << 56) - 1);
	if (sys_config.system_type == SGI_OCTANE && pa == (paddr_t)addr)
		pa = addr - BRIDGE_DMA_DIRECT_BASE;

	if (sys_config.system_type == SGI_OCTANE)
		pa += IP30_MEMORY_BASE;

	return pa;
}
