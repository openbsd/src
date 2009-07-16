/*	$OpenBSD: xbridge.c,v 1.35 2009/07/16 21:02:58 miod Exp $	*/

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
 * XBow Bridge (and XBridge) Widget driver.
 */

/*
 * IMPORTANT AUTHOR'S NOTE: I did not write any of this code under the
 * influence of drugs.  Looking back at that particular piece of hardware,
 * I wonder if this hasn't been a terrible mistake.
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
#include <dev/pci/ppbreg.h>

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
#define	XBRIDGE_FLAGS_XBRIDGE		0x01	/* is XBridge vs Bridge */
#define	XBRIDGE_FLAGS_NO_DIRECT_IO	0x02	/* no direct I/O mapping */
	int16_t		sc_nasid;
	int		sc_widget;
	uint		sc_devio_skew;
	struct mips_pci_chipset sc_pc;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_regh;

	struct mips_bus_space *sc_mem_bus_space;
	struct mips_bus_space *sc_io_bus_space;
	struct machine_bus_dma_tag *sc_dmat;

	struct xbridge_intr	*sc_intr[BRIDGE_NINTRS];

	/*
	 * Device information.
	 */
	struct {
		pcireg_t	id;
		uint32_t	devio;
	} sc_devices[BRIDGE_NSLOTS];

	/*
	 * ATE management.
	 */
	struct mutex	sc_atemtx;
	uint		sc_atecnt;
	struct xbridge_ate	*sc_ate;
	LIST_HEAD(, xbridge_ate) sc_free_ate;
	LIST_HEAD(, xbridge_ate) sc_used_ate;

	/*
	 * Resource extents for the large resource views, used during
	 * resource setup and destroyed afterwards.
	 */
	struct extent	*sc_ioex;
	struct extent	*sc_memex;
};

const struct cfattach xbridge_ca = {
	sizeof(struct xbridge_softc), xbridge_match, xbridge_attach,
};

struct cfdriver xbridge_cd = {
	NULL, "xbridge", DV_DULL,
};

void	xbridge_attach_hook(struct device *, struct device *,
				struct pcibus_attach_args *);
int	xbridge_bus_maxdevs(void *, int);
pcitag_t xbridge_make_tag(void *, int, int, int);
void	xbridge_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t xbridge_conf_read(void *, pcitag_t, int);
void	xbridge_conf_write(void *, pcitag_t, int, pcireg_t);
int	xbridge_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *xbridge_intr_string(void *, pci_intr_handle_t);
void	*xbridge_intr_establish(void *, pci_intr_handle_t, int,
	    int (*func)(void *), void *, char *);
void	xbridge_intr_disestablish(void *, void *);
int	xbridge_ppb_setup(void *, pcitag_t, bus_addr_t *, bus_addr_t *,
	    bus_addr_t *, bus_addr_t *);

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
void	xbridge_ate_dump(struct xbridge_softc *);
uint	xbridge_ate_find(struct xbridge_softc *, paddr_t);
uint64_t xbridge_ate_read(struct xbridge_softc *, uint);
void	xbridge_ate_unref(struct xbridge_softc *, uint, uint);
void	xbridge_ate_write(struct xbridge_softc *, uint, uint64_t);

int	xbridge_allocate_devio(struct xbridge_softc *, int, int);
void	xbridge_set_devio(struct xbridge_softc *, int, uint32_t);

int	xbridge_resource_explore(struct xbridge_softc *, pcitag_t,
	    struct extent *, struct extent *);
void	xbridge_resource_manage(struct xbridge_softc *, pcitag_t,
	    struct extent *, struct extent *);

void	xbridge_ate_setup(struct xbridge_softc *);
void	xbridge_device_setup(struct xbridge_softc *, int, int, uint32_t);
struct extent *
	xbridge_mapping_setup(struct xbridge_softc *, int);
void	xbridge_resource_setup(struct xbridge_softc *);
void	xbridge_rrb_setup(struct xbridge_softc *, int);
void	xbridge_setup(struct xbridge_softc *);

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

	sc->sc_nasid = xaa->xaa_nasid;
	sc->sc_widget = xaa->xaa_widget;

	printf(" revision %d\n", xaa->xaa_revision);
	if (xaa->xaa_vendor == XBOW_VENDOR_SGI3 &&
	    xaa->xaa_product == XBOW_PRODUCT_SGI3_XBRIDGE)
		sc->sc_flags |= XBRIDGE_FLAGS_XBRIDGE;
	else if (xaa->xaa_revision < 4)
		sc->sc_flags |= XBRIDGE_FLAGS_NO_DIRECT_IO;

	/*
	 * Map Bridge registers.
	 */

	sc->sc_iot = malloc(sizeof (*sc->sc_iot), M_DEVBUF, M_NOWAIT);
	if (sc->sc_iot == NULL)
		goto fail0;
	bcopy(xaa->xaa_iot, sc->sc_iot, sizeof (*sc->sc_iot));
	if (bus_space_map(sc->sc_iot, 0, BRIDGE_REGISTERS_SIZE, 0,
	    &sc->sc_regh)) {
		printf("%s: unable to map control registers\n", self->dv_xname);
		free(sc->sc_iot, M_DEVBUF);
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

	bcopy(xaa->xaa_iot, sc->sc_mem_bus_space,
	    sizeof(*sc->sc_mem_bus_space));
	sc->sc_mem_bus_space->bus_private = sc;
	sc->sc_mem_bus_space->_space_map = xbridge_space_map_devio;
	sc->sc_mem_bus_space->_space_read_1 = xbridge_read_1;
	sc->sc_mem_bus_space->_space_write_1 = xbridge_write_1;
	sc->sc_mem_bus_space->_space_read_2 = xbridge_read_2;
	sc->sc_mem_bus_space->_space_write_2 = xbridge_write_2;
	sc->sc_mem_bus_space->_space_read_raw_2 = xbridge_read_raw_2;
	sc->sc_mem_bus_space->_space_write_raw_2 = xbridge_write_raw_2;

	bcopy(xaa->xaa_iot, sc->sc_io_bus_space,
	    sizeof(*sc->sc_io_bus_space));
	sc->sc_io_bus_space->bus_private = sc;
	sc->sc_io_bus_space->_space_map = xbridge_space_map_devio;
	sc->sc_io_bus_space->_space_read_1 = xbridge_read_1;
	sc->sc_io_bus_space->_space_write_1 = xbridge_write_1;
	sc->sc_io_bus_space->_space_read_2 = xbridge_read_2;
	sc->sc_io_bus_space->_space_write_2 = xbridge_write_2;
	sc->sc_io_bus_space->_space_read_raw_2 = xbridge_read_raw_2;
	sc->sc_io_bus_space->_space_write_raw_2 = xbridge_write_raw_2;

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
	sc->sc_pc.pc_ppb_setup = xbridge_ppb_setup;

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
	pba.pba_ioex = NULL;
	pba.pba_memex = NULL;
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
	free(sc->sc_iot, M_DEVBUF);
fail0:
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
		*busp = (tag >> 16) & 0xff;
	if (devp != NULL)
		*devp = (tag >> 11) & 0x1f;
	if (funcp != NULL)
		*funcp = (tag >> 8) & 0x7;
}

int
xbridge_bus_maxdevs(void *cookie, int busno)
{
	return busno == 0 ? BRIDGE_NSLOTS : 32;
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
	if (bus == 0 && sc->sc_devices[dev].id ==
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
	if (bus == 0 && sc->sc_devices[dev].id ==
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

struct xbridge_intrhandler {
	LIST_ENTRY(xbridge_intrhandler)	xih_nxt;
	struct xbridge_intr *xih_main;
	int	(*xih_func)(void *);
	void	*xih_arg;
	struct evcount	xih_count;
	int	 xih_level;
	int	 xih_device;	/* device slot number */
};

struct xbridge_intr {
	struct	xbridge_softc	*xi_bridge;
	int	xi_intrsrc;	/* interrupt source on interrupt widget */
	int	xi_intrbit;	/* interrupt source on BRIDGE */
	LIST_HEAD(, xbridge_intrhandler) xi_handlers;
};

/* how our pci_intr_handle_t are constructed... */
#define	XBRIDGE_INTR_VALID		0x100
#define	XBRIDGE_INTR_HANDLE(d,b)	(XBRIDGE_INTR_VALID | ((d) << 3) | (b))
#define	XBRIDGE_INTR_DEVICE(h)		(((h) >> 3) & 07)
#define	XBRIDGE_INTR_BIT(h)		((h) & 07)

int
xbridge_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct xbridge_softc *sc = pa->pa_pc->pc_conf_v;
	int bus, device, intr;
	int pin;

	*ihp = 0;

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

	if (pa->pa_bridgetag) {
		pin = PPB_INTERRUPT_SWIZZLE(pa->pa_rawintrpin, device);
		if (!ISSET(pa->pa_bridgeih[pin - 1], XBRIDGE_INTR_VALID))
			return 0;
		intr = XBRIDGE_INTR_BIT(pa->pa_bridgeih[pin - 1]);
	} else {
		/*
		 * For IOC devices, the real information is in pa_intrline.
		 */
		if (sc->sc_devices[device].id ==
		    PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3)) {
			intr = pa->pa_intrline;
		} else {
			if (pa->pa_intrpin & 1)
				intr = device;
			else
				intr = device ^ 4;
		}
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
	struct xbridge_softc *sc = (struct xbridge_softc *)cookie;
	struct xbridge_intr *xi;
	struct xbridge_intrhandler *xih;
	uint32_t int_addr;
	int intrbit = XBRIDGE_INTR_BIT(ih);
	int device = XBRIDGE_INTR_DEVICE(ih);
	int intrsrc;
	int new;

	/*
	 * Allocate bookkeeping structure if this is the
	 * first time we're using this interrupt source.
	 */
	if ((xi = sc->sc_intr[intrbit]) == NULL) {
		xi = (struct xbridge_intr *)
		    malloc(sizeof(*xi), M_DEVBUF, M_NOWAIT);
		if (xi == NULL)
			return NULL;

		xi->xi_bridge = sc;
		xi->xi_intrbit = intrbit;
		LIST_INIT(&xi->xi_handlers);

		if (xbow_intr_register(sc->sc_widget, level, &intrsrc) != 0) {
			free(xi, M_DEVBUF);
			return NULL;
		}

		xi->xi_intrsrc = intrsrc;
		sc->sc_intr[intrbit] = xi;
	}
	
	/*
	 * Register the interrupt at the Heart or Hub level if this is the
	 * first time we're using this interrupt source.
	 */
	new = LIST_EMPTY(&xi->xi_handlers);
	if (new) {
		/*
		 * XXX The interrupt dispatcher is always registered
		 * XXX at IPL_TTY, in case the interrupt will be shared
		 * XXX between devices of different levels.
		 */
		if (xbow_intr_establish(xbridge_intr_handler, xi, intrsrc,
		    IPL_TTY, NULL)) {
			printf("%s: unable to register interrupt handler\n",
			    sc->sc_dev.dv_xname);
			return NULL;
		}
	}

	xih = (struct xbridge_intrhandler *)
	    malloc(sizeof(*xih), M_DEVBUF, M_NOWAIT);
	if (xih == NULL)
		return NULL;

	xih->xih_main = xi;
	xih->xih_func = func;
	xih->xih_arg = arg;
	xih->xih_level = level;
	xih->xih_device = device;
	evcount_attach(&xih->xih_count, name, &xi->xi_intrbit, &evcount_intr);
	LIST_INSERT_HEAD(&xi->xi_handlers, xih, xih_nxt);

	if (new) {
		int_addr =
		    ((xbow_intr_widget_register >> 30) & 0x0003ff00) | intrsrc;

		bus_space_write_4(sc->sc_iot, sc->sc_regh,
		    BRIDGE_INT_ADDR(intrbit), int_addr);
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
	}

	return (void *)xih;
}

void
xbridge_intr_disestablish(void *cookie, void *vih)
{
	struct xbridge_softc *sc = cookie;
	struct xbridge_intrhandler *xih = (struct xbridge_intrhandler *)vih;
	struct xbridge_intr *xi = xih->xih_main;
	int intrbit = xi->xi_intrbit;

	evcount_detach(&xih->xih_count);
	LIST_REMOVE(xih, xih_nxt);

	if (LIST_EMPTY(&xi->xi_handlers)) {
		bus_space_write_4(sc->sc_iot, sc->sc_regh,
		    BRIDGE_INT_ADDR(intrbit), 0);
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

		xbow_intr_disestablish(xi->xi_intrsrc);
		/*
		 * Note we could free sc->sc_intr[intrbit] at this point,
		 * but it's not really worth doing.
		 */
	}

	free(xih, M_DEVBUF);
}

int
xbridge_intr_handler(void *v)
{
	struct xbridge_intr *xi = (struct xbridge_intr *)v;
	struct xbridge_softc *sc = xi->xi_bridge;
	struct xbridge_intrhandler *xih;
	int rc = 0;
	int spurious;

	if (LIST_EMPTY(&xi->xi_handlers)) {
		printf("%s: spurious irq %d\n",
		    sc->sc_dev.dv_xname, xi->xi_intrbit);
		return 0;
	}

	/*
	 * Flush PCI write buffers before servicing the interrupt.
	 */
	LIST_FOREACH(xih, &xi->xi_handlers, xih_nxt)
		bus_space_read_4(sc->sc_iot, sc->sc_regh,
		    BRIDGE_DEVICE_WBFLUSH(xih->xih_device));

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

	LIST_FOREACH(xih, &xi->xi_handlers, xih_nxt) {
		if ((*xih->xih_func)(xih->xih_arg) != 0) {
			xih->xih_count.ec_count++;
			rc = 1;
		}
	}
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
	bus_addr_t bpa;
#ifdef DIAGNOSTIC
	bus_addr_t start, end;
	uint d;
#endif

	if ((offs >> 24) != sc->sc_devio_skew)
		return EINVAL;	/* not a devio mapping */

	/*
	 * Figure out which devio `slot' we are using, and make sure
	 * we do not overrun it.
	 */
	bpa = offs & ((1UL << 24) - 1);
#ifdef DIAGNOSTIC
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
#endif

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

	if ((offs >> 24) == sc->sc_devio_skew)
		return xbridge_space_map_devio(t, offs, size, cacheable, bshp);

	*bshp = (t->bus_base + offs);
	return 0;
}

int
xbridge_space_map_mem(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int cacheable, bus_space_handle_t *bshp)
{
	struct xbridge_softc *sc = (struct xbridge_softc *)t->bus_private;

	/*
	 * Base address is either within the devio area, or our direct
	 * window.  Except on Octane where we never setup devio memory
	 * mappings, because the large mapping is always available.
	 */

	if (sys_config.system_type != SGI_OCTANE &&
	    (offs >> 24) == sc->sc_devio_skew)
		return xbridge_space_map_devio(t, offs, size, cacheable, bshp);

	*bshp = (t->bus_base + offs);
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

#ifdef ATE_DEBUG
void
xbridge_ate_dump(struct xbridge_softc *sc)
{
	struct xbridge_ate *ate;
	uint a;

	printf("%s ATE list (in array order)\n", sc->sc_dev.dv_xname);
	for (a = 0, ate = sc->sc_ate; a < sc->sc_atecnt; a++, ate++) {
		printf("%03x %p %02u", a, ate->xa_pa, ate->xa_refcnt);
		if ((a % 3) == 2)
			printf("\n");
		else
			printf("  ");
	}
	if ((a % 3) != 0)
		printf("\n");

	printf("%s USED ATE list (in link order)\n", sc->sc_dev.dv_xname);
	a = 0;
	LIST_FOREACH(ate, &sc->sc_used_ate, xa_nxt) {
		printf("%03x %p %02u",
		    ate - sc->sc_ate, ate->xa_pa, ate->xa_refcnt);
		if ((a % 3) == 2)
			printf("\n");
		else
			printf("  ");
		a++;
	}
	if ((a % 3) != 0)
		printf("\n");

	printf("%s FREE ATE list (in link order)\n", sc->sc_dev.dv_xname);
	a = 0;
	LIST_FOREACH(ate, &sc->sc_free_ate, xa_nxt) {
		printf("%03x %p %02u",
		    ate - sc->sc_ate, ate->xa_pa, ate->xa_refcnt);
		if ((a % 3) == 2)
			printf("\n");
		else
			printf("  ");
		a++;
	}
	if ((a % 3) != 0)
		printf("\n");
}
#endif

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
	    (xbow_intr_widget << ATE_WIDGET_SHIFT) | ATE_COH | ATE_V);

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
		/*
		 * Ask for byteswap during DMA. On Bridge (i.e non-XBridge),
		 * this setting is device-global and is enforced by
		 * BRIDGE_DEVICE_SWAP_PMU set in the devio register.
		 */
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

	printf("%s: out of ATE\n", sc->sc_dev.dv_xname);
#ifdef ATE_DEBUG
	xbridge_ate_dump(sc);
#endif

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

			sgsize = PAGE_SIZE - ((u_long)vaddr & PGOFSET);
			sgsize = min(buflen, sgsize);
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
	uint32_t ctrl;
	int dev;

	/*
	 * Gather device identification for all slots.
	 * We need this to be able to allocate RRBs correctly, and also
	 * to be able to check quickly whether a given device is an IOC3.
	 */

	for (dev = 0; dev < BRIDGE_NSLOTS; dev++) {
		pa = sc->sc_regh + BRIDGE_PCI_CFG_SPACE +
		    (dev << 12) + PCI_ID_REG;
		if (guarded_read_4(pa, &sc->sc_devices[dev].id) != 0)
			sc->sc_devices[dev].id =
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

	/*
	 * Disable byteswapping on PIO accesses through the large window
	 * (we handle this at the bus_space level). It should not have
	 * been enabled by ARCS, since IOC serial console relies on this,
	 * but better enforce this anyway.
	 */

	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_CONTROL);
	ctrl &= ~BRIDGE_WIDGET_CONTROL_IO_SWAP;
	ctrl &= ~BRIDGE_WIDGET_CONTROL_MEM_SWAP;
	bus_space_write_4(sc->sc_iot, sc->sc_regh, WIDGET_CONTROL, ctrl);
	(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);

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
		if (PCI_VENDOR(sc->sc_devices[dev].id) == PCI_VENDOR_INVALID)
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
				if (PCI_VENDOR(sc->sc_devices[dev].id) !=
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

/*
 * Configure PCI resources for all devices.
 */
void
xbridge_resource_setup(struct xbridge_softc *sc)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	int dev, nfuncs;
	pcitag_t tag;
	pcireg_t id, bhlcr;
	uint32_t devio;
	int need_setup;
	uint curppb, nppb;
	const struct pci_quirkdata *qd;

	/*
	 * Figure out where the devio mappings will go.
	 * On Octane, they are relative to the start of the widget.
	 * On Origin, they are computed from the widget number.
	 */

	if (sys_config.system_type == SGI_OCTANE)
		sc->sc_devio_skew = 0;
	else
		sc->sc_devio_skew = sc->sc_widget;

	/*
	 * On Octane, we will want to map everything through the large
	 * windows, whenever possible.
	 *
	 * Set up these mappings now.
	 */

	if (sys_config.system_type == SGI_OCTANE) {
		sc->sc_ioex = xbridge_mapping_setup(sc, 1);
		sc->sc_memex = xbridge_mapping_setup(sc, 0);
	} else

	/*
	 * Configure all regular PCI devices.
	 */

	curppb = nppb = 0;
	for (dev = 0; dev < BRIDGE_NSLOTS; dev++) {
		id = sc->sc_devices[dev].id;

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID || PCI_VENDOR(id) == 0)
			continue;

		/*
		 * Count ppb devices, we will need their number later.
		 */

		tag = pci_make_tag(pc, 0, dev, 0);
		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlcr) == 1)
			nppb++;

		/*
		 * We want to avoid changing mapping configuration for
		 * devices which have been setup by ARCS.
		 *
		 * On Octane, the whole on-board I/O widget has been
		 * set up, with direct mappings into widget space.
		 *
		 * On Origin, since direct mappings are expensive,
		 * everything set up by ARCS has a valid devio
		 * mapping; those can be identified as they sport the
		 * widget number in the high address bits.
		 *
		 * We will only fix the device-global devio flags on
		 * devices which have been set up by ARCS.  Otherwise,
		 * we'll need to perform proper PCI resource allocation.
		 */

		devio = bus_space_read_4(sc->sc_iot, sc->sc_regh,
		    BRIDGE_DEVICE(dev));
#ifdef DEBUG
		printf("device %d: devio %08x\n", dev, devio);
#endif
		if (sys_config.system_type == SGI_OCTANE &&
		    sc->sc_widget == WIDGET_MAX)
			need_setup = 0;
		else
			need_setup = sc->sc_devio_skew !=
			    ((devio & BRIDGE_DEVICE_BASE_MASK) >>
			     (24 - BRIDGE_DEVICE_BASE_SHIFT));

		/*
		 * Enable byte swapping for DMA, except on IOC3 and
		 * RAD1 devices.
		 */
		if (ISSET(sc->sc_flags, XBRIDGE_FLAGS_XBRIDGE))
			devio &= ~BRIDGE_DEVICE_SWAP_PMU;
		else
			devio |= BRIDGE_DEVICE_SWAP_PMU;
		devio |= BRIDGE_DEVICE_SWAP_DIR;
		if (id == PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3) ||
		    id == PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_RAD1))
			devio &=
			    ~(BRIDGE_DEVICE_SWAP_DIR | BRIDGE_DEVICE_SWAP_PMU);

		/*
		 * Disable prefetching - on-board isp(4) controllers on
		 * Octane are set up with this, but this confuses the
		 * driver.
		 */
		devio &= ~BRIDGE_DEVICE_PREFETCH;

		/*
		 * Force cache coherency.
		 */
		devio |= BRIDGE_DEVICE_COHERENT;

		xbridge_set_devio(sc, dev, devio);

		if (need_setup == 0)
			continue;

		/*
		 * Clear any residual devio mapping.
		 */
		devio &= ~BRIDGE_DEVICE_BASE_MASK;
		devio &= ~BRIDGE_DEVICE_IO_MEM;
		xbridge_set_devio(sc, dev, devio);

		/*
		 * We now need to perform the resource allocation for this
		 * device, which has not been setup by ARCS.
		 */

		qd = pci_lookup_quirkdata(PCI_VENDOR(id), PCI_PRODUCT(id));
		if (PCI_HDRTYPE_MULTIFN(bhlcr) ||
		    (qd != NULL && (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0))
			nfuncs = 8;
		else
			nfuncs = 1;

		xbridge_device_setup(sc, dev, nfuncs, devio);
	}

	/*
	 * Configure PCI-PCI bridges, if any.
	 *
	 * We do this after all the other PCI devices have been configured
	 * in order to favour them during resource allocation.
	 */

	for (dev = 0; dev < BRIDGE_NSLOTS; dev++) {
		id = sc->sc_devices[dev].id;

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID || PCI_VENDOR(id) == 0)
			continue;

		tag = pci_make_tag(pc, 0, dev, 0);
		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);

		if (PCI_HDRTYPE_TYPE(bhlcr) != 1)
			continue;

		/*
		 * Being PCI bus #0, we can allocate #1-#255 bus numbers
		 * to the PCI-PCI bridges.
		 * We'll simply split this 255 bus number space accross
		 * all bridges.
		 * Thus, given N bridges on the bus, bridge #M will get
		 * 1 + M * (255/N) .. (M + 1) * (255 / N).
		 */

		ppb_initialize(pc, tag, 1 + curppb * (255 / nppb),
		    (curppb + 1) * (255 / nppb));
		curppb++;
	}

	if (sc->sc_ioex != NULL)
		extent_destroy(sc->sc_ioex);
	if (sc->sc_memex != NULL)
		extent_destroy(sc->sc_memex);
}

struct extent *
xbridge_mapping_setup(struct xbridge_softc *sc, int io)
{
	bus_addr_t offs;
	bus_size_t len;
	paddr_t base;
	u_long start, end;
	struct extent *ex = NULL;

	if (io) {
		/*
		 * I/O mappings are available in the widget at offset
		 * BRIDGE_PCI_IO_SPACE_BASE onwards, but weren't working
		 * correctly until Bridge revision 4 (apparently, what
		 * didn't work was the byteswap logic).
		 */

		if (!ISSET(sc->sc_flags, XBRIDGE_FLAGS_NO_DIRECT_IO)) {
			offs = BRIDGE_PCI_IO_SPACE_BASE;
			len = BRIDGE_PCI_IO_SPACE_LENGTH;
			base = xbow_widget_map_space(sc->sc_dev.dv_parent,
			    sc->sc_widget, &offs, &len);
		} else
			base = 0;

		if (base != 0) {
			if (offs + len > BRIDGE_PCI_IO_SPACE_BASE +
			    BRIDGE_PCI_IO_SPACE_LENGTH)
				len = BRIDGE_PCI_IO_SPACE_BASE +
				    BRIDGE_PCI_IO_SPACE_LENGTH - offs;

#ifdef DEBUG
			printf("direct io %p-%p base %p\n",
			    offs, offs + len - 1, base);
#endif
			offs -= BRIDGE_PCI_IO_SPACE_BASE;

			ex = extent_create("xbridge_direct_io",
			    offs == 0 ? 1 : offs, offs + len - 1,
			    M_DEVBUF, NULL, 0, EX_NOWAIT);

			if (ex != NULL) {
				sc->sc_io_bus_space->bus_base = base - offs;
				sc->sc_io_bus_space->_space_map =
				    xbridge_space_map_io;
			}
		}
	} else {
		/*
		 * Memory mappings are available in the widget at offset
		 * BRIDGE_PCI_MEM_SPACE_BASE onwards.
		 */

		offs = BRIDGE_PCI_MEM_SPACE_BASE;
		len = BRIDGE_PCI_MEM_SPACE_LENGTH;
		base = xbow_widget_map_space(sc->sc_dev.dv_parent,
		    sc->sc_widget, &offs, &len);

		if (base != 0) {
			/*
			 * Only the low 30 bits of memory BAR are honoured
			 * by the hardware, thus restricting memory mappings
			 * to 1GB.
			 */
			if (offs + len > BRIDGE_PCI_MEM_SPACE_BASE +
			    BRIDGE_PCI_MEM_SPACE_LENGTH)
				len = BRIDGE_PCI_MEM_SPACE_BASE +
				    BRIDGE_PCI_MEM_SPACE_LENGTH - offs;

#ifdef DEBUG
			printf("direct mem %p-%p base %p\n",
			    offs, offs + len - 1, base);
#endif
			offs -= BRIDGE_PCI_MEM_SPACE_BASE;

			ex = extent_create("xbridge_direct_mem",
			    offs == 0 ? 1 : offs, offs + len - 1,
			    M_DEVBUF, NULL, 0, EX_NOWAIT);

			if (ex != NULL) {
				sc->sc_mem_bus_space->bus_base = base - offs;
				sc->sc_mem_bus_space->_space_map =
				    xbridge_space_map_mem;
			}
		}
	}

	if (ex != NULL) {
		/*
		 * Remove the devio mapping range from the extent
		 * to avoid ambiguous mappings.
		 *
		 * Note that xbow_widget_map_space() may have returned
		 * a range in which the devio area does not appear.
		 */
#if 0
		start = sc->sc_devio_skew << 24;
		end = start + (1 << 24) - 1;
#else
		/*
		 * Apparently, all addresses under devio need to be
		 * expelled...
		 */
		start = 0;
		end = ((sc->sc_devio_skew + 1) << 24) - 1;
#endif

		if (end >= ex->ex_start && start <= ex->ex_end) {
			if (start < ex->ex_start)
				start = ex->ex_start;
			if (end > ex->ex_end)
				end = ex->ex_end;
			if (extent_alloc_region(ex, start, end - start + 1,
			    EX_NOWAIT | EX_MALLOCOK) != 0) {
				printf("%s: failed to expurge devio range"
				    " from %s large extent\n",
				    sc->sc_dev.dv_xname,
				    io ? "i/o" : "mem");
				extent_destroy(ex);
				ex = NULL;
			}
		}
	}

	return ex;
}

/*
 * Flags returned by xbridge_resource_explore()
 */
#define	XR_IO		0x01	/* needs I/O mappings */
#define	XR_MEM		0x02	/* needs memory mappings */
#define	XR_IO_OFLOW_S	0x04	/* can't fit I/O in a short devio */
#define	XR_MEM_OFLOW_S	0x08	/* can't fit memory in a short devio */
#define	XR_IO_OFLOW	0x10	/* can't fit I/O in a large devio */
#define	XR_MEM_OFLOW	0x20	/* can't fit memory in a large devio */

int
xbridge_resource_explore(struct xbridge_softc *sc, pcitag_t tag,
    struct extent *ioex, struct extent *memex)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	pcireg_t bhlc, type;
	bus_addr_t base;
	bus_size_t size;
	int reg, reg_start, reg_end;
	int rc = 0;

	bhlc = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(bhlc)) {
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
		return rc;
	}

	for (reg = reg_start; reg < reg_end; reg += 4) {
		if (pci_mapreg_probe(pc, tag, reg, &type) == 0)
			continue;

		if (pci_mapreg_info(pc, tag, reg, type, NULL, &size, NULL))
			continue;

		switch (type) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			reg += 4;
			/* FALLTHROUGH */
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
			if (memex != NULL) {
				rc |= XR_MEM;
				if (size > memex->ex_end - memex->ex_start)
					rc |= XR_MEM_OFLOW | XR_MEM_OFLOW_S;
				else if (extent_alloc(memex, size, size,
				    0, 0, 0, &base) != 0)
					rc |= XR_MEM_OFLOW | XR_MEM_OFLOW_S;
				else if (base >= BRIDGE_DEVIO_SHORT)
					rc |= XR_MEM_OFLOW_S;
			} else
				rc |= XR_MEM | XR_MEM_OFLOW | XR_MEM_OFLOW_S;
			break;
		case PCI_MAPREG_TYPE_IO:
			if (ioex != NULL) {
				rc |= XR_IO;
				if (size > ioex->ex_end - ioex->ex_start)
					rc |= XR_IO_OFLOW | XR_IO_OFLOW_S;
				else if (extent_alloc(ioex, size, size,
				    0, 0, 0, &base) != 0)
					rc |= XR_IO_OFLOW | XR_IO_OFLOW_S;
				else if (base >= BRIDGE_DEVIO_SHORT)
					rc |= XR_IO_OFLOW_S;
			} else
				rc |= XR_IO | XR_IO_OFLOW | XR_IO_OFLOW_S;
			break;
		}
	}

	return rc;
}

void
xbridge_resource_manage(struct xbridge_softc *sc, pcitag_t tag,
    struct extent *ioex, struct extent *memex)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	pcireg_t bhlc, type;
	bus_addr_t base;
	bus_size_t size;
	int reg, reg_start, reg_end;

	bhlc = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(bhlc)) {
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

		/*
		 * Note that we do not care about the existing BAR values,
		 * since these devices either have not been setup by ARCS
		 * or do not matter for early system setup (such as
		 * optional IOC3 PCI boards, which will get setup by
		 * ARCS but can be reinitialized as we see fit).
		 */
#ifdef DEBUG
		printf("bar %02x type %d base %p size %p",
		    reg, type, base, size);
#endif
		switch (type) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			/*
			 * Since our mapping ranges are restricted to
			 * at most 30 bits, the upper part of the 64 bit
			 * BAR registers is always zero.
			 */
			pci_conf_write(pc, tag, reg + 4, 0);
			/* FALLTHROUGH */
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
			if (memex != NULL) {
				if (extent_alloc(memex, size, size, 0, 0, 0,
				    &base) != 0)
					base = 0;
			} else
				base = 0;
			break;
		case PCI_MAPREG_TYPE_IO:
			if (ioex != NULL) {
				if (extent_alloc(ioex, size, size, 0, 0, 0,
				    &base) != 0)
					base = 0;
			} else
				base = 0;
			break;
		}

#ifdef DEBUG
		printf(" setup at %p\n", base);
#endif
		pci_conf_write(pc, tag, reg, base);

		if (type == (PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT))
			reg += 4;
	}
}

void
xbridge_device_setup(struct xbridge_softc *sc, int dev, int nfuncs,
    uint32_t devio)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	int function;
	pcitag_t tag;
	pcireg_t id, csr;
	uint32_t basewin;
	int resources;
	int io_devio, mem_devio;
	struct extent *ioex, *memex;

	/*
	 * In a first step, we enumerate all the requested resources,
	 * and check if they could fit within devio mappings.
	 *
	 * If devio can't afford us the mappings we need, we'll
	 * try and allocate a large window.
	 */

	/*
	 * Allocate extents to use for devio mappings if necessary.
	 * This can fail; in that case we'll try to use a large mapping
	 * whenever possible, or silently fail to configure the device.
	 */
	if (sc->sc_ioex != NULL)
		ioex = NULL;
	else
		ioex = extent_create("xbridge_io",
		    0, BRIDGE_DEVIO_LARGE - 1,
		    M_DEVBUF, NULL, 0, EX_NOWAIT);
	if (sc->sc_memex != NULL)
		memex = NULL;
	else
		memex = extent_create("xbridge_mem",
		    0, BRIDGE_DEVIO_LARGE - 1,
		    M_DEVBUF, NULL, 0, EX_NOWAIT);

	resources = 0;
	for (function = 0; function < nfuncs; function++) {
		tag = pci_make_tag(pc, 0, dev, function);
		id = pci_conf_read(pc, tag, PCI_ID_REG);

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr &
		    ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE));

		resources |= xbridge_resource_explore(sc, tag, ioex, memex);
	}

	if (memex != NULL) {
		extent_destroy(memex);
		memex = NULL;
	}
	if (ioex != NULL) {
		extent_destroy(ioex);
		ioex = NULL;
	}

	/*
	 * In a second step, if resources can be mapped using devio slots,
	 * allocate them. Otherwise, or if we can't get a devio slot
	 * big enough for the resources we need to map, we'll need
	 * to get a large window mapping.
	 *
	 * Note that, on Octane, we try to avoid using devio whenever
	 * possible.
	 */

	io_devio = -1;
	if (ISSET(resources, XR_IO)) {
		if (!ISSET(resources, XR_IO_OFLOW) &&
		    (sys_config.system_type != SGI_OCTANE ||
		     sc->sc_ioex == NULL))
			io_devio = xbridge_allocate_devio(sc, dev,
			    ISSET(resources, XR_IO_OFLOW_S));
		if (io_devio >= 0) {
			basewin = (sc->sc_devio_skew << 24) |
			    BRIDGE_DEVIO_OFFS(io_devio);
			xbridge_set_devio(sc, io_devio, devio |
			    (basewin >> BRIDGE_DEVICE_BASE_SHIFT));

			ioex = extent_create("xbridge_io", basewin,
			    basewin + BRIDGE_DEVIO_SIZE(io_devio) - 1,
			    M_DEVBUF, NULL, 0, EX_NOWAIT);
		} else {
			/*
			 * Try to get a large window mapping if we don't
			 * have one already.
			 */
			if (sc->sc_ioex == NULL)
				sc->sc_ioex = xbridge_mapping_setup(sc, 1);
		}
	}

	mem_devio = -1;
	if (ISSET(resources, XR_MEM)) {
		if (!ISSET(resources, XR_MEM_OFLOW) &&
		    sys_config.system_type != SGI_OCTANE)
			mem_devio = xbridge_allocate_devio(sc, dev,
			    ISSET(resources, XR_MEM_OFLOW_S));
		if (mem_devio >= 0) {
			basewin = (sc->sc_devio_skew << 24) |
			    BRIDGE_DEVIO_OFFS(mem_devio);
			xbridge_set_devio(sc, mem_devio, devio |
			    BRIDGE_DEVICE_IO_MEM |
			    (basewin >> BRIDGE_DEVICE_BASE_SHIFT));

			memex = extent_create("xbridge_mem", basewin,
			    basewin + BRIDGE_DEVIO_SIZE(mem_devio) - 1,
			    M_DEVBUF, NULL, 0, EX_NOWAIT);
		} else {
			/*
			 * Try to get a large window mapping if we don't
			 * have one already.
			 */
			if (sc->sc_memex == NULL)
				sc->sc_memex = xbridge_mapping_setup(sc, 0);
		}
	}

	/*
	 * Finally allocate the resources proper and update the
	 * device BARs accordingly.
	 */

	for (function = 0; function < nfuncs; function++) {
		tag = pci_make_tag(pc, 0, dev, function);
		id = pci_conf_read(pc, tag, PCI_ID_REG);

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		xbridge_resource_manage(sc, tag,
		    ioex != NULL ? ioex : sc->sc_ioex,
		    memex != NULL ?  memex : sc->sc_memex);
	}

	if (memex != NULL)
		extent_destroy(memex);
	if (ioex != NULL)
		extent_destroy(ioex);
}

int
xbridge_ppb_setup(void *cookie, pcitag_t tag, bus_addr_t *iostart,
    bus_addr_t *ioend, bus_addr_t *memstart, bus_addr_t *memend)
{
	struct xbridge_softc *sc = cookie;
	pci_chipset_tag_t pc = &sc->sc_pc;
	uint32_t base, devio;
	bus_size_t exsize;
	u_long exstart;
	int dev, devio_idx, tries;

	pci_decompose_tag(pc, tag, NULL, &dev, NULL);
	devio = bus_space_read_4(sc->sc_iot, sc->sc_regh, BRIDGE_DEVICE(dev));

	/*
	 * Since our caller computes resource needs starting at zero, we
	 * can ignore the start values when computing the amount of
	 * resources we'll need.
	 */

	exsize = *memend;
	*memstart = 0xffffffff;
	*memend = 0;
	if (exsize++ != 0) {
		/* try to allocate through a devio slot whenever possible... */
		if (exsize < BRIDGE_DEVIO_SHORT)
			devio_idx = xbridge_allocate_devio(sc, dev, 0);
		else if (exsize < BRIDGE_DEVIO_LARGE)
			devio_idx = xbridge_allocate_devio(sc, dev, 1);
		else
			devio_idx = -1;

		/* ...if it fails, try the large view.... */
		if (devio_idx < 0 && sc->sc_memex == NULL)
			sc->sc_memex = xbridge_mapping_setup(sc, 0);

		/* ...if it is not available, try to get a devio slot anyway. */
		if (devio_idx < 0 && sc->sc_memex == NULL) {
			if (exsize > BRIDGE_DEVIO_SHORT)
				devio_idx = xbridge_allocate_devio(sc, dev, 1);
			if (devio_idx < 0)
				devio_idx = xbridge_allocate_devio(sc, dev, 0);
		}

		if (devio_idx >= 0) {
			base = (sc->sc_devio_skew << 24) |
			    BRIDGE_DEVIO_OFFS(devio_idx);
			xbridge_set_devio(sc, devio_idx, devio |
			    BRIDGE_DEVICE_IO_MEM |
			    (base >> BRIDGE_DEVICE_BASE_SHIFT));
			*memstart = base;
			*memend = base + BRIDGE_DEVIO_SIZE(devio_idx) - 1;
		} else {
			/*
			 * We know that the direct memory resource range fits
			 * within the 32 bit address space, and is limited to
			 * 30 bits, so our allocation, if successfull, will
			 * work as a 32 bit memory range.
			 */
			if (exsize < 1UL << 20)
				exsize = 1UL << 20;
			for (tries = 0; tries < 5; tries++) {
				if (extent_alloc(sc->sc_memex, exsize,
				    1UL << 20, 0, 0, EX_NOWAIT | EX_MALLOCOK,
				    &exstart) == 0) {
					*memstart = exstart;
					*memend = exstart + exsize - 1;
					break;
				}
				exsize >>= 1;
				if (exsize < 1UL << 20)
					break;
			}
		}
	}

	exsize = *ioend;
	*iostart = 0xffffffff;
	*ioend = 0;
	if (exsize++ != 0) {
		/* try to allocate through a devio slot whenever possible... */
		if (exsize < BRIDGE_DEVIO_SHORT)
			devio_idx = xbridge_allocate_devio(sc, dev, 0);
		else if (exsize < BRIDGE_DEVIO_LARGE)
			devio_idx = xbridge_allocate_devio(sc, dev, 1);
		else
			devio_idx = -1;

		/* ...if it fails, try the large view.... */
		if (devio_idx < 0 && sc->sc_ioex == NULL)
			sc->sc_ioex = xbridge_mapping_setup(sc, 1);

		/* ...if it is not available, try to get a devio slot anyway. */
		if (devio_idx < 0 && sc->sc_ioex == NULL) {
			if (exsize > BRIDGE_DEVIO_SHORT)
				devio_idx = xbridge_allocate_devio(sc, dev, 1);
			if (devio_idx < 0)
				devio_idx = xbridge_allocate_devio(sc, dev, 0);
		}

		if (devio_idx >= 0) {
			base = (sc->sc_devio_skew << 24) |
			    BRIDGE_DEVIO_OFFS(devio_idx);
			xbridge_set_devio(sc, devio_idx, devio |
			    (base >> BRIDGE_DEVICE_BASE_SHIFT));
			*iostart = base;
			*ioend = base + BRIDGE_DEVIO_SIZE(devio_idx) - 1;
		} else {
			/*
			 * We know that the direct I/O resource range fits
			 * within the 32 bit address space, so our allocation,
			 * if successfull, will work as a 32 bit i/o range.
			 */
			if (exsize < 1UL << 12)
				exsize = 1UL << 12;
			for (tries = 0; tries < 5; tries++) {
				if (extent_alloc(sc->sc_ioex, exsize,
				    1UL << 12, 0, 0, EX_NOWAIT | EX_MALLOCOK,
				    &exstart) == 0) {
					*iostart = exstart;
					*ioend = exstart + exsize - 1;
					break;
				}
				exsize >>= 1;
				if (exsize < 1UL << 12)
					break;
			}
		}
	}

	return 0;
}

int
xbridge_allocate_devio(struct xbridge_softc *sc, int dev, int wantlarge)
{
	pcireg_t id;

	/*
	 * If the preferred slot is available and matches the size requested,
	 * use it.
	 */

	if (sc->sc_devices[dev].devio == 0) {
		if (BRIDGE_DEVIO_SIZE(dev) >=
		    wantlarge ? BRIDGE_DEVIO_LARGE : BRIDGE_DEVIO_SHORT)
			return dev;
	}

	/*
	 * Otherwise pick the smallest available devio matching our size
	 * request.
	 */

	for (dev = 0; dev < BRIDGE_NSLOTS; dev++) {
		if (sc->sc_devices[dev].devio != 0)
			continue;	/* devio in use */

		id = sc->sc_devices[dev].id;
		if (PCI_VENDOR(id) != PCI_VENDOR_INVALID && PCI_VENDOR(id) != 0)
			continue;	/* devio to be used soon */

		if (BRIDGE_DEVIO_SIZE(dev) >=
		    wantlarge ? BRIDGE_DEVIO_LARGE : BRIDGE_DEVIO_SHORT)
			return dev;
	}

	return -1;
}

void
xbridge_set_devio(struct xbridge_softc *sc, int dev, uint32_t devio)
{
	bus_space_write_4(sc->sc_iot, sc->sc_regh, BRIDGE_DEVICE(dev), devio);
	(void)bus_space_read_4(sc->sc_iot, sc->sc_regh, WIDGET_TFLUSH);
	sc->sc_devices[dev].devio = devio;
#ifdef DEBUG
	printf("device %d: new devio %08x\n", dev, devio);
#endif
}
