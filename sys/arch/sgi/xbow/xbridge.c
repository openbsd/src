/*	$OpenBSD: xbridge.c,v 1.30 2009/06/28 21:52:54 miod Exp $	*/

/*
 * Copyright (c) 2008, 2009  Miodrag Vallat.
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
#include <sys/extent.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/queue.h>

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/mnode.h>

#include <uvm/uvm_extern.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <mips64/archtype.h>
#include <sgi/xbow/hub.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>

#include <sgi/xbow/xbridgereg.h>

#include <sgi/sgi/ip30.h>

int	xbridge_match(struct device *, void *, void *);
void	xbridge_attach(struct device *, struct device *, void *);
int	xbridge_print(void *, const char *);

struct xbridge_intr;
struct xbridge_ate;

struct xbridge_softc {
	struct device	sc_dev;
	int		sc_flags;
#define	XBRIDGE_FLAGS_XBRIDGE	0x01	/* is XBridge vs Bridge */
	int16_t		sc_nasid;
	int		sc_widget;
	struct mips_pci_chipset sc_pc;

	struct mips_bus_space *sc_mem_bus_space;
	struct mips_bus_space *sc_io_bus_space;
	struct machine_bus_dma_tag *sc_dmat;
	struct extent	*sc_mem_ex;
	struct extent	*sc_io_ex;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_regh;

	int		sc_intrbit[BRIDGE_NINTRS];
	struct xbridge_intr	*sc_intr[BRIDGE_NINTRS];

	pcireg_t	sc_devices[BRIDGE_NSLOTS];

	struct mutex	sc_atemtx;
	uint		sc_atecnt;
	struct xbridge_ate	*sc_ate;
	LIST_HEAD(, xbridge_ate) sc_free_ate;
	LIST_HEAD(, xbridge_ate) sc_used_ate;
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

int	xbridge_space_map_devio(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	xbridge_space_map_io(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	xbridge_space_map_mem(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);

int	xbridge_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
	    struct proc *, int);
int	xbridge_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t, struct mbuf *,
	    int);
int	xbridge_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t, struct uio *, int);
void	xbridge_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
int	xbridge_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t,
	    bus_dma_segment_t *, int, int *, int);
bus_addr_t xbridge_pa_to_device(paddr_t);
paddr_t	xbridge_device_to_pa(bus_addr_t);

int	xbridge_address_map(struct xbridge_softc *, paddr_t, bus_addr_t *,
	    bus_addr_t *);
void	xbridge_address_unmap(struct xbridge_softc *, bus_addr_t, bus_size_t);
uint	xbridge_ate_add(struct xbridge_softc *, paddr_t);
uint	xbridge_ate_find(struct xbridge_softc *, paddr_t);
uint64_t xbridge_ate_read(struct xbridge_softc *, uint);
void	xbridge_ate_unref(struct xbridge_softc *, uint, uint);
void	xbridge_ate_write(struct xbridge_softc *, uint, uint64_t);

void	xbridge_setup(struct xbridge_softc *);

void	xbridge_ate_setup(struct xbridge_softc *);
void	xbridge_resource_explore(struct xbridge_softc *, pcitag_t,
	    int *, int *);
void	xbridge_resource_manage(struct xbridge_softc *, pcitag_t,
	    struct extent *, int);
void	xbridge_resource_setup(struct xbridge_softc *);
void	xbridge_rrb_setup(struct xbridge_softc *, int);

const struct machine_bus_dma_tag xbridge_dma_tag = {
	NULL,			/* _cookie */
	_dmamap_create,
	_dmamap_destroy,
	xbridge_dmamap_load,
	xbridge_dmamap_load_mbuf,
	xbridge_dmamap_load_uio,
	_dmamap_load_raw,
	xbridge_dmamap_unload,
	_dmamap_sync,
	xbridge_dmamem_alloc,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap,
	xbridge_pa_to_device,
	xbridge_device_to_pa,
	BRIDGE_DMA_DIRECT_LENGTH - 1
};

/*
 ********************* Autoconf glue.
 */

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
	int direct_io_avail = 0;

	sc->sc_nasid = xaa->xaa_nasid;
	sc->sc_widget = xaa->xaa_widget;

	printf(" revision %d\n", xaa->xaa_revision);
	if (xaa->xaa_vendor == XBOW_VENDOR_SGI3 &&
	    xaa->xaa_product == XBOW_PRODUCT_SGI3_XBRIDGE)
		sc->sc_flags |= XBRIDGE_FLAGS_XBRIDGE;

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

#ifdef notyet
	/* Unrestricted memory mappings in the large window */
	bcopy(xaa->xaa_long_tag, sc->sc_mem_bus_space,
	    sizeof(*sc->sc_mem_bus_space));
	sc->sc_mem_ex = extent_create("pcimem",
	    0, BRIDGE_PCI_MEM_SPACE_LENGTH - 1,
	    M_DEVBUF, NULL, 0, EX_NOWAIT);
	sc->sc_mem_bus_space->_space_map = xbridge_space_map_mem;
#else
	/* Programmable memory mappings in the small window */
	bcopy(xaa->xaa_short_tag, sc->sc_mem_bus_space,
	    sizeof(*sc->sc_mem_bus_space));
	sc->sc_mem_bus_space->_space_map = xbridge_space_map_devio;
#endif

	if (ISSET(sc->sc_flags, XBRIDGE_FLAGS_XBRIDGE) ||
	    xaa->xaa_revision >= 4) {
		switch (sys_config.system_type) {
		default:
#if defined(TGT_ORIGIN200) || defined(TGT_ORIGIN2000)
		case SGI_O200:
		case SGI_O300:
			/*
			 * In N mode, the large window is truncated and the
			 * direct I/O area is not accessible.
			 */
			if (kl_n_mode == 0)
				direct_io_avail = 1;
			break;
#endif
#if defined(TGT_OCTANE)
		case SGI_OCTANE:
			direct_io_avail = 1;
			break;
#endif
		}
	}
#ifdef notyet
	if (direct_io_avail) {
#else
	if (0) {
#endif
		/* Unrestricted I/O mappings in the large window */
		bcopy(xaa->xaa_long_tag, sc->sc_io_bus_space,
		    sizeof(*sc->sc_io_bus_space));
		sc->sc_io_ex = extent_create("pciio",
		    0, BRIDGE_PCI_IO_SPACE_LENGTH - 1,
		    M_DEVBUF, NULL, 0, EX_NOWAIT);
		sc->sc_io_bus_space->_space_map = xbridge_space_map_io;
	} else {
		/* Programmable I/O mappings in the small window */
		bcopy(xaa->xaa_short_tag, sc->sc_io_bus_space,
		    sizeof(*sc->sc_io_bus_space));
		sc->sc_io_bus_space->_space_map = xbridge_space_map_devio;
	}

	sc->sc_io_bus_space->bus_private = sc;
	sc->sc_io_bus_space->_space_read_1 = xbridge_read_1;
	sc->sc_io_bus_space->_space_write_1 = xbridge_write_1;
	sc->sc_io_bus_space->_space_read_2 = xbridge_read_2;
	sc->sc_io_bus_space->_space_write_2 = xbridge_write_2;
	sc->sc_io_bus_space->_space_read_raw_2 = xbridge_read_raw_2;
	sc->sc_io_bus_space->_space_write_raw_2 = xbridge_write_raw_2;

	sc->sc_mem_bus_space->bus_private = sc;
	sc->sc_mem_bus_space->_space_read_1 = xbridge_read_1;
	sc->sc_mem_bus_space->_space_write_1 = xbridge_write_1;
	sc->sc_mem_bus_space->_space_read_2 = xbridge_read_2;
	sc->sc_mem_bus_space->_space_write_2 = xbridge_write_2;
	sc->sc_mem_bus_space->_space_read_raw_2 = xbridge_read_raw_2;
	sc->sc_mem_bus_space->_space_write_raw_2 = xbridge_write_raw_2;

	sc->sc_dmat = malloc(sizeof (*sc->sc_dmat), M_DEVBUF, M_NOWAIT);
	if (sc->sc_dmat == NULL)
		goto fail3;
	memcpy(sc->sc_dmat, &xbridge_dma_tag, sizeof(*sc->sc_dmat));
	sc->sc_dmat->_cookie = sc;

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
	 * Configure Bridge for proper operation (DMA, I/O mappings,
	 * RRB allocation, etc).
	 */

	xbridge_setup(sc);

	/*
	 * Attach children.
	 */

	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = sc->sc_io_bus_space;
	pba.pba_memt = sc->sc_mem_bus_space;
	pba.pba_dmat = sc->sc_dmat;
	pba.pba_ioex = sc->sc_io_ex;
	pba.pba_memex = sc->sc_mem_ex;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0;

	config_found(self, &pba, xbridge_print);
	return;

fail3:
	free(sc->sc_io_bus_space, M_DEVBUF);
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

/*
 ********************* PCI glue.
 */

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
	pcireg_t data;
	int bus, dev, fn;
	paddr_t pa;
	int skip;
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
	 * registers (supposedly only the first 0x20 bytes, however
	 * writing to the second BAR also writes to the first).
	 *
	 * Depending on which particular model we encounter, things may
	 * seem to work, or write access to nonexisting registers would
	 * completely freeze the machine.
	 *
	 * We thus check for the device type here, and handle the non
	 * existing registers ourselves.
	 */

	skip = 0;
	if (bus == 0 && sc->sc_devices[dev] ==
	    PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3)) {
		switch (offset) {
		case PCI_ID_REG:
		case PCI_COMMAND_STATUS_REG:
		case PCI_CLASS_REG:
		case PCI_BHLC_REG:
		case PCI_MAPREG_START:
			/* These registers are implemented. Go ahead. */
			break;
		case PCI_INTERRUPT_REG:
			/* This register is not implemented. Fake it. */
			data = (PCI_INTERRUPT_PIN_A <<
			    PCI_INTERRUPT_PIN_SHIFT) |
			    (dev << PCI_INTERRUPT_LINE_SHIFT);
			skip = 1;
			break;
		default:
			/* These registers are not implemented. */
			data = 0;
			skip = 1;
			break;
		}
	}

	if (skip == 0) {
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
	int bus, dev, fn;
	paddr_t pa;
	int skip;
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

	skip = 0;
	if (bus == 0 && sc->sc_devices[dev] ==
	    PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3)) {
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
			break;
		default:
			/* These registers are not implemented. */
			skip = 1;
			break;
		}
	}

	if (skip == 0) {
		pa += (fn << 8) + offset;
		guarded_write_4(pa, data);
	}

	splx(s);
}

/*
 ********************* Interrupt handling.
 */

/*
 * We map each slot to its own interrupt bit, which will in turn be routed to
 * the Heart or Hub widget in charge of interrupt processing.
 */

struct xbridge_intr {
	struct	xbridge_softc	*xi_bridge;
	int	xi_intrsrc;	/* interrupt source on interrupt widget */
	int	xi_intrbit;	/* interrupt source on BRIDGE */
	int	xi_device;	/* device slot number */

	int	(*xi_func)(void *);
	void	*xi_arg;

	struct evcount	xi_count;
	int	 xi_level;
};

/* how our pci_intr_handle_t are constructed... */
#define	XBRIDGE_INTR_HANDLE(d,b)	(0x100 | ((d) << 3) | (b))
#define	XBRIDGE_INTR_DEVICE(h)		(((h) >> 3) & 07)
#define	XBRIDGE_INTR_BIT(h)		((h) & 07)

int
xbridge_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct xbridge_softc *sc = pa->pa_pc->pc_conf_v;
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

	/*
	 * For IOC devices, the real information is in pa_intrline.
	 */
	if (sc->sc_devices[device] ==
	    PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3)) {
		intr = pa->pa_intrline;
	} else {
		if (pa->pa_intrpin & 1)
			intr = device;
		else
			intr = device ^ 4;
	}

	*ihp = XBRIDGE_INTR_HANDLE(device, intr);

	return 0;
}

const char *
xbridge_intr_string(void *cookie, pci_intr_handle_t ih)
{
	static char str[16];

	snprintf(str, sizeof(str), "irq %d", XBRIDGE_INTR_BIT(ih));
	return(str);
}

void *
xbridge_intr_establish(void *cookie, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct xbridge_softc *sc = cookie;
	struct xbridge_intr *xi;
	uint32_t int_addr;
	int intrbit = XBRIDGE_INTR_BIT(ih);
	int device = XBRIDGE_INTR_DEVICE(ih);
	int intrsrc;

	/*
	 * XXX At worst, there can only be two interrupt handlers registered
	 * XXX on the same pin.
	 */
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
		    level, NULL)) {
			printf("%s: unable to register interrupt handler, "
			    "did xheart or xhub attach?\n",
			    sc->sc_dev.dv_xname);
			return NULL;
		}

		sc->sc_intrbit[intrbit] = intrsrc;
	}

	xi->xi_bridge = sc;
	xi->xi_intrsrc = intrsrc;
	xi->xi_intrbit = intrbit;
	xi->xi_device = device;
	xi->xi_func = func;
	xi->xi_arg = arg;
	xi->xi_level = level;
	evcount_attach(&xi->xi_count, name, &xi->xi_level, &evcount_intr);
	sc->sc_intr[intrbit] = xi;

	int_addr = ((xbow_intr_widget_register >> 30) & 0x0003ff00) | intrsrc;

	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_ADDR(intrbit),
	    int_addr);
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_IER,
	    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_IER) |
	    (1 << intrbit));
	/*
	 * INT_MODE register controls which interrupt pins cause
	 * ``interrupt clear'' packets to be sent for high->low
	 * transition.
	 * We enable such packets to be sent in order not to have to
	 * clear interrupts ourselves.
	 */
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_MODE,
	    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_MODE) |
	    (1 << intrbit));
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_DEV,
	    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_DEV) |
	    (device << (intrbit * 3)));
	(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);

	return (void *)((uint64_t)ih);
}

void
xbridge_intr_disestablish(void *cookie, void *vih)
{
	struct xbridge_softc *sc = cookie;
	struct xbridge_intr *xi;
	pci_intr_handle_t ih = (pci_intr_handle_t)(uint64_t)vih;
	int intrbit = XBRIDGE_INTR_BIT(ih);

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
	int spurious;

	if (xi == NULL) {
		printf("%s: spurious irq %d\n",
		    sc->sc_dev.dv_xname, xi->xi_intrbit);
		return 0;
	}

	/*
	 * Flush PCI write buffers before servicing the interrupt.
	 */
	bus_space_read_4(sc->sc_iot, sc->sc_regh,
	    BRIDGE_DEVICE_WBFLUSH(xi->xi_device));

	if ((bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_ISR) &
	    (1 << xi->xi_intrbit)) == 0) {
		spurious = 1;
#ifdef DEBUG
		printf("%s: irq %d but not pending in ISR %08x\n",
		    sc->sc_dev.dv_xname, xi->xi_intrbit,
		    bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_ISR));
#endif
	} else
		spurious = 0;

	if ((rc = (*xi->xi_func)(xi->xi_arg)) != 0)
		xi->xi_count.ec_count++;
	if (rc == 0 && spurious == 0)
		printf("%s: spurious irq %d\n",
		    sc->sc_dev.dv_xname, xi->xi_intrbit);

	/*
	 * There is a known BRIDGE race in which, if two interrupts
	 * on two different pins occur within 60nS of each other,
	 * further interrupts on the first pin do not cause an
	 * interrupt to be sent.
	 *
	 * The workaround against this is to check if our interrupt
	 * source is still active (i.e. another interrupt is pending),
	 * in which case we force an interrupt anyway.
	 *
	 * The XBridge even has a nice facility to do this, where we
	 * do not even have to check if our interrupt is pending.
	 */

	if (ISSET(sc->sc_flags, XBRIDGE_FLAGS_XBRIDGE)) {
		bus_space_write_4(sc->sc_iot, sc->sc_regh,
		    BRIDGE_INT_FORCE_PIN(xi->xi_intrbit), 1);
	} else {
		if (bus_space_read_4(sc->sc_iot, sc->sc_regh,
		    BRIDGE_ISR) & (1 << xi->xi_intrbit)) {
			switch (sys_config.system_type) {
#if defined(TGT_OCTANE)
			case SGI_OCTANE:
				/* XXX what to do there? */
				break;
#endif
#if defined(TGT_ORIGIN200) || defined(TGT_ORIGIN2000)
			case SGI_O200:
			case SGI_O300:
				IP27_RHUB_PI_S(sc->sc_nasid, 0, HUBPI_IR_CHANGE,
				    PI_IR_SET | xi->xi_intrsrc);
				break;
#endif
			}
		}
	}

	return 1;
}

/*
 ********************* bus_space glue.
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

int
xbridge_space_map_devio(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int cacheable, bus_space_handle_t *bshp)
{
	struct xbridge_softc *sc = (struct xbridge_softc *)t->bus_private;
	bus_addr_t bpa, start, end;
	uint d;

	if ((offs >> 24) != sc->sc_widget)
		return EINVAL;	/* not a devio mapping */

	/*
	 * Figure out which devio `slot' we are using, and make sure
	 * we do not overrun it.
	 */
	bpa = offs & ((1UL << 24) - 1);
	for (d = 0; d < BRIDGE_NSLOTS; d++) {
		start = BRIDGE_DEVIO_OFFS(d);
		end = start + BRIDGE_DEVIO_SIZE(d);
		if (bpa >= start && bpa < end) {
			if (bpa + size > end)
				return EINVAL;
			else
				break;
		}
	}
	if (d == BRIDGE_NSLOTS)
		return EINVAL;

	/*
	 * Note we can not use our own bus_base because it might not point
	 * to our small window. Instead, use the one used by the xbridge
	 * driver itself, which _always_ points to the short window.
	 */
	*bshp = sc->sc_iot->bus_base + bpa;
	return 0;
}

int
xbridge_space_map_io(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int cacheable, bus_space_handle_t *bshp)
{
	struct xbridge_softc *sc = (struct xbridge_softc *)t->bus_private;

	/*
	 * Base address is either within the devio area, or our direct
	 * window.
	 */

	if ((offs >> 24) == sc->sc_widget)
		return xbridge_space_map_devio(t, offs, size, cacheable, bshp);

	/* check that this doesn't overflow the window */
	if (offs + size > BRIDGE_PCI_IO_SPACE_LENGTH || offs + size < offs)
		return EINVAL;

	*bshp = t->bus_base + BRIDGE_PCI_IO_SPACE_BASE + offs;
	return 0;
}

int
xbridge_space_map_mem(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int cacheable, bus_space_handle_t *bshp)
{
	struct xbridge_softc *sc = (struct xbridge_softc *)t->bus_private;

	/*
	 * Base address is either within the devio area, or our direct
	 * window.
	 */

	if ((offs >> 24) == sc->sc_widget)
		return xbridge_space_map_devio(t, offs, size, cacheable, bshp);

	/* check that this doesn't overflow the window */
	if (offs + size > BRIDGE_PCI_MEM_SPACE_LENGTH || offs + size < offs)
		return EINVAL;

	*bshp = t->bus_base + BRIDGE_PCI_MEM_SPACE_BASE + offs;
	return 0;
}

/*
 ********************* bus_dma helpers
 */

/*
 * ATE primer:
 *
 * ATE are iommu translation entries. PCI addresses in the translated
 * window transparently map to the address their ATE point to.
 *
 * Bridge chip have 128 so-called `internal' entries, and can use their
 * optional SSRAM to provide more (up to 65536 entries with 512KB SSRAM).
 * However, due to chip bugs, those `external' entries can not be updated
 * while there is DMA in progress using external entries, even if the
 * updated entries are disjoint from those used by the DMA transfer.
 *
 * XBridge chip extend the internal entries to 1024, and do not provide
 * support for external entries.
 *
 * We limit ourselves to internal entries only. Due to the way we force
 * bus_dmamem_alloc() to use the direct window, there won't hopefully be
 * many concurrent consumers of ATE at once.
 *
 * All ATE share the same page size, which is configurable as 4KB or 16KB.
 * In order to minimize the number of ATE used by the various drivers,
 * we use 16KB pages, at the expense of trickier code to account for
 * ATE shared by various dma maps.
 *
 * ATE management:
 *
 * An array of internal ATE management structures is allocated, and
 * provides reference counters (since various dma maps could overlap
 * the same 16KB ATE pages).
 *
 * When using ATE in the various bus_dmamap_load*() functions, we try
 * to coalesce individual contiguous pages sharing the same I/O page
 * (and thus the same ATE). However, no attempt is made to optimize
 * entries using contiguous ATEs.
 *
 * ATE are organized in lists of in-use and free entries.
 */

struct xbridge_ate {
	LIST_ENTRY(xbridge_ate)	 xa_nxt;
	uint			 xa_refcnt;
	paddr_t			 xa_pa;
};

void
xbridge_ate_setup(struct xbridge_softc *sc)
{
	uint32_t ctrl;
	uint a;
	struct xbridge_ate *ate;

	mtx_init(&sc->sc_atemtx, IPL_HIGH);

	if (ISSET(sc->sc_flags, XBRIDGE_FLAGS_XBRIDGE))
		sc->sc_atecnt = XBRIDGE_INTERNAL_ATE;
	else
		sc->sc_atecnt = BRIDGE_INTERNAL_ATE;

	sc->sc_ate = (struct xbridge_ate *)malloc(sc->sc_atecnt *
	    sizeof(struct xbridge_ate), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (sc->sc_ate == NULL) {
		/* we could run without, but this would be a PITA */
		panic("%s: no memory for ATE management", __func__);
	}

	/*
	 * Setup the ATE lists.
	 */
	LIST_INIT(&sc->sc_free_ate);
	LIST_INIT(&sc->sc_used_ate);
	for (ate = sc->sc_ate; ate != sc->sc_ate + sc->sc_atecnt; ate++)
		LIST_INSERT_HEAD(&sc->sc_free_ate, ate, xa_nxt);

	/*
	 * Switch to 16KB pages.
	 */
	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_CONTROL);
	bus_space_write_4(sc->sc_iot, sc->sc_regh, WIDGET_CONTROL,
	    ctrl | BRIDGE_WIDGET_CONTROL_LARGE_PAGES);
	(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);

	/*
	 * Initialize all ATE entries to invalid.
	 */
	for (a = 0; a < sc->sc_atecnt; a++)
		xbridge_ate_write(sc, a, ATE_NV);
}

#ifdef unused
uint64_t
xbridge_ate_read(struct xbridge_softc *sc, uint a)
{
	uint32_t lo, hi;
	uint64_t ate;

	/*
	 * ATE can not be read as a whole, and need two 32 bit accesses.
	 */
	hi = bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_ATE(a) + 4);
	if (ISSET(sc->sc_flags, XBRIDGE_FLAGS_XBRIDGE))
		lo = bus_space_read_4(sc->sc_iot, sc->sc_regh,
		    BRIDGE_ATE(a + 1024) + 4);
	else
		lo = bus_space_read_4(sc->sc_iot, sc->sc_regh,
		    BRIDGE_ATE(a + 512) + 4);

	ate = (uint64_t)hi;
	ate <<= 32;
	ate |= lo;

	return ate;
}
#endif

void
xbridge_ate_write(struct xbridge_softc *sc, uint a, uint64_t ate)
{
	bus_space_write_8(sc->sc_iot, sc->sc_regh, BRIDGE_ATE(a), ate);
}

uint
xbridge_ate_find(struct xbridge_softc *sc, paddr_t pa)
{
	uint a;
	struct xbridge_ate *ate;

	/* round to ATE page */
	pa &= ~BRIDGE_ATE_LMASK;

	/*
	 * XXX Might want to build a tree to make this faster than
	 * XXX that stupid linear search. On the other hand there
	 * XXX aren't many ATE entries.
	 */
	LIST_FOREACH(ate, &sc->sc_used_ate, xa_nxt)
		if (ate->xa_pa == pa) {
			a = ate - sc->sc_ate;
#ifdef ATE_DEBUG
			printf("%s: pa %p ate %u (r %u)\n",
			    __func__, pa, a, ate->xa_refcnt);
#endif
			return a;
		}

	return (uint)-1;
}

uint
xbridge_ate_add(struct xbridge_softc *sc, paddr_t pa)
{
	uint a;
	struct xbridge_ate *ate;

	/* round to ATE page */
	pa &= ~BRIDGE_ATE_LMASK;

	if (LIST_EMPTY(&sc->sc_free_ate)) {
#ifdef ATE_DEBUG
		printf("%s: out of ATEs\n", sc->sc_dev.dv_xname);
#endif
		return (uint)-1;
	}

	ate = LIST_FIRST(&sc->sc_free_ate);
	LIST_REMOVE(ate, xa_nxt);
	LIST_INSERT_HEAD(&sc->sc_used_ate, ate, xa_nxt);
	ate->xa_refcnt = 1;
	ate->xa_pa = pa;

	a = ate - sc->sc_ate;
#ifdef ATE_DEBUG
	printf("%s: pa %p ate %u\n", __func__, pa, a);
#endif

	xbridge_ate_write(sc, a, ate->xa_pa |
	    (xbow_intr_widget << ATE_WIDGET_SHIFT) | ATE_V);

	return a;
}

void
xbridge_ate_unref(struct xbridge_softc *sc, uint a, uint ref)
{
	struct xbridge_ate *ate;

	ate = sc->sc_ate + a;
#ifdef DIAGNOSTIC
	if (ref > ate->xa_refcnt)
		panic("%s: ate #%u %p has only %u refs but needs to drop %u",
		    sc->sc_dev.dv_xname, a, ate, ate->xa_refcnt, ref);
#endif
	ate->xa_refcnt -= ref;
	if (ate->xa_refcnt == 0) {
#ifdef ATE_DEBUG
		printf("%s: free ate %u\n", __func__, a);
#endif
		xbridge_ate_write(sc, a, ATE_NV);
		LIST_REMOVE(ate, xa_nxt);
		LIST_INSERT_HEAD(&sc->sc_free_ate, ate, xa_nxt);
	} else {
#ifdef ATE_DEBUG
		printf("%s: unref ate %u (r %u)\n", __func__, a, ate->xa_refcnt);
#endif
	}
}

/*
 * Attempt to map the given address, either through the direct map, or
 * using an ATE.
 */
int
xbridge_address_map(struct xbridge_softc *sc, paddr_t pa, bus_addr_t *mapping,
    bus_addr_t *limit)
{
	struct xbridge_ate *ate;
	bus_addr_t ba;
	uint a;

	/*
	 * Try the direct DMA window first.
	 */

#ifdef TGT_OCTANE
	if (sys_config.system_type == SGI_OCTANE)
		ba = (bus_addr_t)pa - IP30_MEMORY_BASE;
	else
#endif
		ba = (bus_addr_t)pa;

	if (ba < BRIDGE_DMA_DIRECT_LENGTH) {
		*mapping = ba + BRIDGE_DMA_DIRECT_BASE;
		*limit = BRIDGE_DMA_DIRECT_LENGTH + BRIDGE_DMA_DIRECT_BASE;
		return 0;
	}

	/*
	 * Did not fit, so now we need to use an ATE.
	 * Check if an existing ATE would do the job; if not, try and
	 * allocate a new one.
	 */

	mtx_enter(&sc->sc_atemtx);

	a = xbridge_ate_find(sc, pa);
	if (a != (uint)-1) {
		ate = sc->sc_ate + a;
		ate->xa_refcnt++;
	} else
		a = xbridge_ate_add(sc, pa);

	if (a != (uint)-1) {
		ba = ATE_ADDRESS(a, BRIDGE_ATE_LSHIFT);
		if (ISSET(sc->sc_flags, XBRIDGE_FLAGS_XBRIDGE))
			ba |= XBRIDGE_DMA_TRANSLATED_SWAP;
#ifdef ATE_DEBUG
		printf("%s: ate %u through %p\n", __func__, a, ba);
#endif
		*mapping = ba + (pa & BRIDGE_ATE_LMASK);
		*limit = ba + BRIDGE_ATE_LSIZE;
		mtx_leave(&sc->sc_atemtx);
		return 0;
	}

	mtx_leave(&sc->sc_atemtx);

	/*
	 * We could try allocating a bounce buffer here.
	 * Maybe once there is a MI interface for this...
	 */

	return EINVAL;
}

void
xbridge_address_unmap(struct xbridge_softc *sc, bus_addr_t ba, bus_size_t len)
{
	uint a;
	uint refs;

	/*
	 * If this address matches an ATE, unref it, and make it
	 * available again if the reference count drops to zero.
	 */
	if (ba < BRIDGE_DMA_TRANSLATED_BASE || ba >= BRIDGE_DMA_DIRECT_BASE)
		return;

	if (ba & XBRIDGE_DMA_TRANSLATED_SWAP)
		ba &= ~XBRIDGE_DMA_TRANSLATED_SWAP;

	a = ATE_INDEX(ba, BRIDGE_ATE_LSHIFT);
#ifdef DIAGNOSTIC
	if (a >= sc->sc_atecnt)
		panic("%s: bus address %p references nonexisting ATE %u/%u",
		    __func__, ba, a, sc->sc_atecnt);
#endif

	/*
	 * Since we only coalesce contiguous pages or page fragments in
	 * the maps, and we made sure not to cross I/O page boundaries,
	 * we have one reference per cpu page the range [ba, ba+len-1]
	 * hits.
	 */
	refs = 1 + atop(ba + len - 1) - atop(ba);

	mtx_enter(&sc->sc_atemtx);
	xbridge_ate_unref(sc, a, refs);
	mtx_leave(&sc->sc_atemtx);
}

/*
 * bus_dmamap_load() implementation.
 */
int
xbridge_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	struct xbridge_softc *sc = t->_cookie;
	paddr_t pa;
	bus_addr_t lastaddr, busaddr, endaddr;
	bus_size_t sgsize;
	bus_addr_t baddr, bmask;
	caddr_t vaddr = buf;
	int first, seg;
	pmap_t pmap;
	bus_size_t saved_buflen;
	int rc;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
	for (seg = 0; seg < map->_dm_segcnt; seg++)
		map->dm_segs[seg].ds_addr = 0;

	if (buflen > map->_dm_size)
		return EINVAL;

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	bmask  = ~(map->_dm_boundary - 1);
	if (t->_dma_mask != 0)
		bmask &= t->_dma_mask;

	saved_buflen = buflen;
	for (first = 1, seg = 0; buflen > 0; ) {
		/*
		 * Get the physical address for this segment.
		 */
		if (pmap_extract(pmap, (vaddr_t)vaddr, &pa) == FALSE)
			panic("%s: pmap_extract(%x, %x) failed",
			    __func__, pmap, vaddr);

		/*
		 * Compute the DMA address and the physical range 
		 * this mapping can cover.
		 */
		if (xbridge_address_map(sc, pa, &busaddr, &endaddr) != 0) {
			rc = ENOMEM;
			goto fail_unmap;
		}

		/*
		 * Compute the segment size, and adjust counts.
		 * Note that we do not min() against (endaddr - busaddr)
		 * as the initial sgsize computation is <= (endaddr - busaddr).
		 */
		sgsize = PAGE_SIZE - ((u_long)vaddr & PGOFSET);
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (map->_dm_boundary > 0) {
			baddr = (pa + map->_dm_boundary) & bmask;
			if (sgsize > (baddr - pa))
				sgsize = baddr - pa;
		}

		/*
		 * Insert chunk into a segment, coalescing with
		 * previous segment if possible.
		 */
		if (first) {
			map->dm_segs[seg].ds_addr = busaddr;
			map->dm_segs[seg].ds_len = sgsize;
			map->dm_segs[seg]._ds_vaddr = (vaddr_t)vaddr;
			first = 0;
		} else {
			if (busaddr == lastaddr &&
			    (map->dm_segs[seg].ds_len + sgsize) <=
			     map->_dm_maxsegsz &&
			     (map->_dm_boundary == 0 ||
			     (map->dm_segs[seg].ds_addr & bmask) ==
			     (busaddr & bmask)))
				map->dm_segs[seg].ds_len += sgsize;
			else {
				if (++seg >= map->_dm_segcnt) {
					/* drop partial ATE reference */
					xbridge_address_unmap(sc, busaddr,
					    sgsize);
					break;
				}
				map->dm_segs[seg].ds_addr = busaddr;
				map->dm_segs[seg].ds_len = sgsize;
				map->dm_segs[seg]._ds_vaddr = (vaddr_t)vaddr;
			}
		}

		lastaddr = busaddr + sgsize;
		if (lastaddr == endaddr)
			lastaddr = ~0;	/* can't coalesce */
		vaddr += sgsize;
		buflen -= sgsize;
	}

	/*
	 * Did we fit?
	 */
	if (buflen != 0) {
		rc = EFBIG;
		goto fail_unmap;
	}

	map->dm_nsegs = seg + 1;
	map->dm_mapsize = saved_buflen;
	return 0;

fail_unmap:
	/*
	 * If control goes there, we need to unref all our ATE, if any.
	 */
	for (seg = 0; seg < map->_dm_segcnt; seg++) {
		xbridge_address_unmap(sc, map->dm_segs[seg].ds_addr,
		    map->dm_segs[seg].ds_len);
		map->dm_segs[seg].ds_addr = 0;
	}

	return rc;
}

/*
 * bus_dmamap_load_mbuf() implementation.
 */
int
xbridge_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *m,
    int flags)
{
	struct xbridge_softc *sc = t->_cookie;
	bus_addr_t lastaddr, busaddr, endaddr;
	bus_size_t sgsize;
	paddr_t pa;
	vaddr_t lastva;
	int seg;
	size_t len;
	int rc;

	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
	for (seg = 0; seg < map->_dm_segcnt; seg++)
		map->dm_segs[seg].ds_addr = 0;

	seg = 0;
	len = 0;
	while (m != NULL) {
		vaddr_t vaddr = mtod(m, vaddr_t);
		long buflen = (long)m->m_len;

		len += buflen;
		while (buflen > 0 && seg < map->_dm_segcnt) {
			if (pmap_extract(pmap_kernel(), vaddr, &pa) == FALSE)
				panic("%s: pmap_extract(%x, %x) failed",
				    __func__, pmap_kernel(), vaddr);

			/*
			 * Compute the DMA address and the physical range
			 * this mapping can cover.
			 */
			if (xbridge_address_map(sc, pa, &busaddr,
			    &endaddr) != 0) {
				rc = ENOMEM;
				goto fail_unmap;
			}

			sgsize = min(buflen, PAGE_SIZE);
			sgsize = min(endaddr - busaddr, sgsize);

			/*
			 * Try to coalesce with previous entry.
			 * We need both the physical addresses and
			 * the virtual address to be contiguous, for
			 * bus_dmamap_sync() to behave correctly.
			 */
			if (seg > 0 &&
			    busaddr == lastaddr && vaddr == lastva &&
			    (map->dm_segs[seg - 1].ds_len + sgsize <=
			     map->_dm_maxsegsz))
				map->dm_segs[seg - 1].ds_len += sgsize;
			else {
				map->dm_segs[seg].ds_addr = busaddr;
				map->dm_segs[seg].ds_len = sgsize;
				map->dm_segs[seg]._ds_vaddr = vaddr;
				seg++;
			}

			lastaddr = busaddr + sgsize;
			if (lastaddr == endaddr)
				lastaddr = ~0;	/* can't coalesce */
			vaddr += sgsize;
			lastva = vaddr;
			buflen -= sgsize;
		}
		m = m->m_next;
		if (m && seg >= map->_dm_segcnt) {
			/* Exceeded the size of our dmamap */
			rc = EFBIG;
			goto fail_unmap;
		}
	}
	map->dm_nsegs = seg;
	map->dm_mapsize = len;
	return 0;

fail_unmap:
	/*
	 * If control goes there, we need to unref all our ATE, if any.
	 */
	for (seg = 0; seg < map->_dm_segcnt; seg++) {
		xbridge_address_unmap(sc, map->dm_segs[seg].ds_addr,
		    map->dm_segs[seg].ds_len);
		map->dm_segs[seg].ds_addr = 0;
	}

	return rc;
}

/*
 * bus_dmamap_load_uio() non-implementation.
 */
int
xbridge_dmamap_load_uio(bus_dma_tag_t t, bus_dmamap_t map, struct uio *uio,
    int flags)
{
	return EOPNOTSUPP;
}

/*
 * bus_dmamap_unload() implementation.
 */
void
xbridge_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct xbridge_softc *sc = t->_cookie;
	int seg;

	for (seg = 0; seg < map->_dm_segcnt; seg++) {
		xbridge_address_unmap(sc, map->dm_segs[seg].ds_addr,
		    map->dm_segs[seg].ds_len);
		map->dm_segs[seg].ds_addr = 0;
	}
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
}

/*
 * bus_dmamem_alloc() implementation.
 */
int
xbridge_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{
	paddr_t low, high;

	/*
	 * Limit bus_dma'able memory to the first 2GB of physical memory.
	 * XXX This should be lifted if flags & BUS_DMA_64BIT for drivers
	 * XXX which do not need to restrict themselves to 32 bit DMA
	 * XXX addresses.
	 */
	switch (sys_config.system_type) {
	default:
#if defined(TGT_ORIGIN200) || defined(TGT_ORIGIN2000)
	case SGI_O200:
	case SGI_O300:
		low = 0;
		break;
#endif
#if defined(TGT_OCTANE)
	case SGI_OCTANE:
		low = IP30_MEMORY_BASE;
		break;
#endif
	}
	high = low + BRIDGE_DMA_DIRECT_LENGTH - 1;

	return _dmamem_alloc_range(t, size, alignment, boundary,
	    segs, nsegs, rsegs, flags, low, high);
}

/*
 * Since we override the various bus_dmamap_load*() functions, the only
 * caller of pa_to_device() and device_to_pa() is _dmamem_alloc_range(),
 * invoked by xbridge_dmamem_alloc() above. Since we make sure this
 * function can only return memory fitting in the direct DMA window, we do
 * not need to check for other cases.
 */

bus_addr_t
xbridge_pa_to_device(paddr_t pa)
{
#ifdef TGT_OCTANE
	if (sys_config.system_type == SGI_OCTANE)
		pa -= IP30_MEMORY_BASE;
#endif

	return pa + BRIDGE_DMA_DIRECT_BASE;
}

paddr_t
xbridge_device_to_pa(bus_addr_t addr)
{
	paddr_t pa = addr - BRIDGE_DMA_DIRECT_BASE;

#ifdef TGT_OCTANE
	if (sys_config.system_type == SGI_OCTANE)
		pa += IP30_MEMORY_BASE;
#endif

	return pa;
}

/*
 ********************* Bridge configuration code.
 */

void
xbridge_setup(struct xbridge_softc *sc)
{
	paddr_t pa;
	int dev, i;

	/*
	 * Gather device identification for all slots.
	 * We need this to be able to allocate RRBs correctly, and also
	 * to be able to check quickly whether a given device is an IOC3.
	 */

	for (dev = 0; dev < BRIDGE_NSLOTS; dev++) {
		pa = sc->sc_regh + BRIDGE_PCI_CFG_SPACE +
		    (dev << 12) + PCI_ID_REG;
		if (guarded_read_4(pa, &sc->sc_devices[dev]) != 0)
			sc->sc_devices[dev] =
			    PCI_ID_CODE(PCI_VENDOR_INVALID, 0xffff);
	}

	/*
	 * Configure the direct DMA window to access the low 2GB of memory.
	 * XXX assumes masternasid is 0
	 */

	if (sys_config.system_type == SGI_OCTANE)
		bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_DIR_MAP,
		    (xbow_intr_widget << BRIDGE_DIRMAP_WIDGET_SHIFT) |
		    BRIDGE_DIRMAP_ADD_512MB);
	else
		bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_DIR_MAP,
		    xbow_intr_widget << BRIDGE_DIRMAP_WIDGET_SHIFT);

	/*
	 * Figure out how many ATE we can use for non-direct DMA, and
	 * setup our ATE management code.
	 */

	xbridge_ate_setup(sc);

	/*
	 * Allocate RRB for the existing devices.
	 */

	xbridge_rrb_setup(sc, 0);
	xbridge_rrb_setup(sc, 1);

#ifdef notyet
	/*
	 * Enable byteswapping on accesses through the large window,
	 * except on the main I/O widget on Octane, where the default
	 * mappings require them to be disabled (which doesn't matter,
	 * since the contents of the PCI bus are immutable and well-known).
	 */

	if (sys_config.system_type != SGI_OCTANE ||
	    sc->sc_widget != WIDGET_MAX) {
		uint32_t ctrl = bus_space_read_4(sc->sc_iot, sc->sc_regh,
		    WIDGET_CONTROL);
		ctrl |= BRIDGE_WIDGET_CONTROL_IO_SWAP;
		ctrl |= BRIDGE_WIDGET_CONTROL_MEM_SWAP;
		bus_space_write_4(sc->sc_iot, sc->sc_regh, WIDGET_CONTROL,
		    ctrl);
		(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);
	}
#endif

	/*
	 * The PROM will only configure the onboard devices. Set up
	 * any other device we might encounter.
	 */

	xbridge_resource_setup(sc);

	/*
	 * Setup interrupt handling.
	 */

	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_IER, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_MODE, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_INT_DEV, 0);

	bus_space_write_4(sc->sc_iot, sc->sc_regh, WIDGET_INTDEST_ADDR_UPPER,
	    (xbow_intr_widget_register >> 32) | (xbow_intr_widget << 16));
	bus_space_write_4(sc->sc_iot, sc->sc_regh, WIDGET_INTDEST_ADDR_LOWER,
	    (uint32_t)xbow_intr_widget_register);

	(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);

	for (i = 0; i < BRIDGE_NINTRS; i++)
		sc->sc_intrbit[i] = -1;
}

/*
 * Build a not-so-pessimistic RRB allocation register value.
 */
void
xbridge_rrb_setup(struct xbridge_softc *sc, int odd)
{
	uint rrb[BRIDGE_NSLOTS / 2];	/* tentative rrb assignment */
	uint total;			/* rrb count */
	uint32_t proto;			/* proto rrb value */
	int dev, i, j;

	/*
	 * First, try to allocate as many RRBs per device as possible.
	 */

	total = 0;
	for (i = 0; i < BRIDGE_NSLOTS / 2; i++) {
		dev = (i << 1) + !!odd;
		if (PCI_VENDOR(sc->sc_devices[dev]) == PCI_VENDOR_INVALID)
			rrb[i] = 0;
		else
			rrb[i] = 4;	/* optimistic value */
		total += rrb[i];
	}

	/*
	 * Then, try to reduce greed until we do not claim more than
	 * the 8 RRBs we can afford.
	 */

	if (total > 8) {
		/*
		 * All devices should be able to live with 3 RRBs, so
		 * reduce their allocation from 4 to 3.
		 */
		for (i = 0; i < BRIDGE_NSLOTS / 2; i++) {
			if (rrb[i] == 4) {
				rrb[i]--;
				if (--total == 8)
					break;
			}
		}
	}

	if (total > 8) {
		/*
		 * There are too many devices for 3 RRBs per device to
		 * be possible. Attempt to reduce from 3 to 2, except
		 * for isp(4) devices.
		 */
		for (i = 0; i < BRIDGE_NSLOTS / 2; i++) {
			if (rrb[i] == 3) {
				dev = (i << 1) + !!odd;
				if (PCI_VENDOR(sc->sc_devices[dev]) !=
				    PCI_VENDOR_QLOGIC) {
					rrb[i]--;
					if (--total == 8)
						break;
				}
			}
		}
	}

	if (total > 8) {
		/*
		 * Too bad, we need to shrink the RRB allocation for
		 * isp devices too. We'll try to favour the lowest
		 * slots, though, hence the reversed loop order.
		 */
		for (i = BRIDGE_NSLOTS / 2 - 1; i >= 0; i--) {
			if (rrb[i] == 3) {
				rrb[i]--;
				if (--total == 8)
					break;
			}
		}
	}

	/*
	 * Now build the RRB register value proper.
	 */

	proto = 0;
	for (i = 0; i < BRIDGE_NSLOTS / 2; i++) {
		for (j = 0; j < rrb[i]; j++)
			proto = (proto << RRB_SHIFT) | (RRB_VALID | i);
	}

	bus_space_write_4(sc->sc_iot, sc->sc_regh,
	    odd ? BRIDGE_RRB_ODD : BRIDGE_RRB_EVEN, proto);
}

void
xbridge_resource_setup(struct xbridge_softc *sc)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	int dev, function, nfuncs;
	pcitag_t tag;
	pcireg_t id, bhlcr;
	const struct pci_quirkdata *qd;
	uint32_t devio, basewin;
	int io, mem;
	int need_setup;
	struct extent *ex;

	for (dev = 0; dev < BRIDGE_NSLOTS; dev++) {
		id = sc->sc_devices[dev];

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID || PCI_VENDOR(id) == 0)
			continue;

		/*
		 * Devices which have been configured by the firmware
		 * have their I/O window pointing to the bridge widget.
		 * XXX We only need to preserve IOC3 devio settings if
		 * XXX it is the console.
		 */
		devio = bus_space_read_4(sc->sc_iot, sc->sc_regh,
		    BRIDGE_DEVICE(dev));
		need_setup = ((devio & BRIDGE_DEVICE_BASE_MASK) >>
		    (24 - BRIDGE_DEVICE_BASE_SHIFT)) != sc->sc_widget;

		/*
		 * On Octane, the firmware will setup the I/O registers
		 * correctly for the on-board devices, except for byteswap.
		 * Other PCI buses, and other systems, need more attention.
		 */
		if (sys_config.system_type == SGI_OCTANE &&
		    sc->sc_widget == WIDGET_MAX)
			need_setup = 0;

		if (need_setup) {
			basewin =
			    (sc->sc_widget << 24) | BRIDGE_DEVIO_OFFS(dev);

			devio &= ~BRIDGE_DEVICE_BASE_MASK;

			/*
			 * XXX This defaults to I/O resources only.
			 * XXX However some devices may carry only
			 * XXX memory mappings.
			 * XXX This code should assign devio in a more
			 * XXX flexible way...
			 */
			devio &= ~BRIDGE_DEVICE_IO_MEM;

			devio |= (basewin >> BRIDGE_DEVICE_BASE_SHIFT);
		}

		/*
		 * Enable byte swapping for DMA, except on IOC3 and
		 * RAD1 devices.
		 */
		devio &= ~(BRIDGE_DEVICE_SWAP_DIR | BRIDGE_DEVICE_SWAP_PMU);
		if (id != PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3) &&
		    id != PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_RAD1))
			devio |= BRIDGE_DEVICE_SWAP_PMU;

		bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_DEVICE(dev),
		    devio);
		(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);

		/*
		 * If we can manage I/O and memory resource allocation in
		 * the MI code, we do not need to do anything more at this
		 * stage...
		 */

		if (sc->sc_io_ex != NULL && sc->sc_mem_ex != NULL)
			continue;

		/*
		 * ...otherwise, we need to perform the resource allocation
		 * ourselves, within the devio window we have configured
		 * above, for the devices which have not been setup by the
		 * firmware already.
		 */

		if (need_setup == 0)
			continue;

		ex = extent_create("pcires",
		    basewin, basewin + BRIDGE_DEVIO_SIZE(dev) - 1,
		    M_DEVBUF, NULL, 0, EX_NOWAIT);
		if (ex == NULL)
			continue;

		qd = pci_lookup_quirkdata(PCI_VENDOR(id), PCI_PRODUCT(id));
		tag = pci_make_tag(pc, 0, dev, 0);
		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);

		if (PCI_HDRTYPE_MULTIFN(bhlcr) ||
		    (qd != NULL && (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0))
			nfuncs = 8;
		else
			nfuncs = 1;

		/*
		 * Count how many I/O and memory mappings are necessary.
		 */

		io = mem = 0;

		for (function = 0; function < nfuncs; function++) {
			tag = pci_make_tag(pc, 0, dev, function);
			id = pci_conf_read(pc, tag, PCI_ID_REG);

			if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
			    PCI_VENDOR(id) == 0)
				continue;

			xbridge_resource_explore(sc, tag, &io, &mem);
		}

		/*
		 * As long as the memory area in the large window 
		 * isn't working as well as we'd like it to,
		 * we can only use devio mappings in the short window.
		 *
		 * For devices having both I/O and memory resources, we
		 * favour the I/O resources so far. Eventually this code
		 * should attempt to steal a devio from an unpopulated
		 * slot.
		 */

		if (io == 0 && mem != 0) {
			/* swap devio type */
			devio |= BRIDGE_DEVICE_IO_MEM;
			bus_space_write_4(sc->sc_iot, sc->sc_regh,
			    BRIDGE_DEVICE(dev), devio);
			(void)bus_space_read_4(sc->sc_iot, sc->sc_regh,
			    WIDGET_TFLUSH);
		} else
			mem = 0;

		for (function = 0; function < nfuncs; function++) {
			tag = pci_make_tag(pc, 0, dev, function);
			id = pci_conf_read(pc, tag, PCI_ID_REG);

			if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
			    PCI_VENDOR(id) == 0)
				continue;

			xbridge_resource_manage(sc, tag, ex, mem != 0);
		}

		extent_destroy(ex);
	}
}

void
xbridge_resource_explore(struct xbridge_softc *sc, pcitag_t tag,
    int *nio, int *nmem)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	pcireg_t bhlc, type;
	int reg, reg_start, reg_end;

	bhlc = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE(bhlc)) {
	case 0:
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_END;
		break;
	case 1:	/* PCI-PCI bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PPB_END;
		break;
	case 2:	/* PCI-CardBus bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PCB_END;
		break;
	default:
		return;
	}

	for (reg = reg_start; reg < reg_end; reg += 4) {
		if (pci_mapreg_probe(pc, tag, reg, &type) == 0)
			continue;

		if (pci_mapreg_info(pc, tag, reg, type, NULL, NULL, NULL))
			continue;

		switch (type) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			(*nmem)++;
			break;
		case PCI_MAPREG_TYPE_IO:
			(*nio)++;
			break;
		}

		if (type & PCI_MAPREG_MEM_TYPE_64BIT)
			reg += 4;
	}
}

void
xbridge_resource_manage(struct xbridge_softc *sc, pcitag_t tag,
    struct extent *ex, int prefer_mem)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	pcireg_t bhlc, type;
	bus_addr_t base;
	bus_size_t size;
	int reg, reg_start, reg_end;

	bhlc = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE(bhlc)) {
	case 0:
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_END;
		break;
	case 1:	/* PCI-PCI bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PPB_END;
		break;
	case 2:	/* PCI-CardBus bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PCB_END;
		break;
	default:
		return;
	}

	for (reg = reg_start; reg < reg_end; reg += 4) {
		if (pci_mapreg_probe(pc, tag, reg, &type) == 0)
			continue;

		if (pci_mapreg_info(pc, tag, reg, type, &base, &size, NULL))
			continue;

		switch (type) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			if (sc->sc_mem_ex == NULL && prefer_mem != 0) {
				if (extent_alloc(ex, size, size, 0, 0, 0,
				    &base) != 0)
					base = 0;
			} else
				base = 0;
			break;
		case PCI_MAPREG_TYPE_IO:
			if (sc->sc_io_ex == NULL && prefer_mem == 0) {
				if (base != 0 && base >= ex->ex_start &&
				    base + size - 1 <= ex->ex_end) {
					if (extent_alloc_region(ex, base, size,
					    EX_NOWAIT)) {
						printf("io address conflict"
						    " 0x%x/0x%x\n", base, size);
						base = 0;
					}
				} else {
					if (extent_alloc(ex, size, size, 0, 0,
					    0, &base) != 0)
						base = 0;
				}
			} else
				base = 0;
			break;
		}

		pci_conf_write(pc, tag, reg, base);

		if (type & PCI_MAPREG_MEM_TYPE_64BIT)
			reg += 4;
	}
}
