/*	$OpenBSD: macepcibridge.c,v 1.23 2009/07/16 21:02:56 miod Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB (www.opsycon.se)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Machine dependent PCI BUS Bridge driver on Mace (O2).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/extent.h>
#include <uvm/uvm.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/vmparam.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/ppbreg.h>
#include <dev/pci/pcidevs.h>

#include <mips64/archtype.h>
#include <sgi/localbus/crimebus.h>
#include <sgi/localbus/macebus.h>
#include <sgi/pci/macepcibrvar.h>

int	 mace_pcibrmatch(struct device *, void *, void *);
void	 mace_pcibrattach(struct device *, struct device *, void *);

void	 mace_pcibr_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	 mace_pcibr_bus_maxdevs(void *, int);
pcitag_t mace_pcibr_make_tag(void *, int, int, int);
void	 mace_pcibr_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t mace_pcibr_conf_read(void *, pcitag_t, int);
void	 mace_pcibr_conf_write(void *, pcitag_t, int, pcireg_t);
int	 mace_pcibr_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *mace_pcibr_intr_string(void *, pci_intr_handle_t);
void	*mace_pcibr_intr_establish(void *, pci_intr_handle_t, int,
	    int (*)(void *), void *, char *);
void	 mace_pcibr_intr_disestablish(void *, void *);

bus_addr_t mace_pcibr_pa_to_device(paddr_t);
paddr_t	 mace_pcibr_device_to_pa(bus_addr_t);

void	 mace_pcibr_configure(struct mace_pcibr_softc *);
int	 mace_pcibr_errintr(void *);
int	 mace_pcibr_ppb_setup(void *, pcitag_t, bus_addr_t *, bus_addr_t *,
	    bus_addr_t *, bus_addr_t *);

extern void pciaddr_remap(pci_chipset_tag_t);

struct cfattach macepcibr_ca = {
	sizeof(struct mace_pcibr_softc), mace_pcibrmatch, mace_pcibrattach,
};

struct cfdriver macepcibr_cd = {
	NULL, "macepcibr", DV_DULL,
};

long pci_io_ext_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof (long)];
long pci_mem_ext_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof (long)];

bus_space_t mace_pcibbus_mem_tag = {
	NULL,
	(bus_addr_t)MACE_PCI_MEM_BASE,
	NULL,
	0,
	mace_pcib_read_1, mace_pcib_write_1,
	mace_pcib_read_2, mace_pcib_write_2,
	mace_pcib_read_4, mace_pcib_write_4,
	mace_pcib_read_8, mace_pcib_write_8,
	mace_pcib_read_raw_2, mace_pcib_write_raw_2,
	mace_pcib_read_raw_4, mace_pcib_write_raw_4,
	mace_pcib_read_raw_8, mace_pcib_write_raw_8,
	mace_pcib_space_map, mace_pcib_space_unmap, mace_pcib_space_region,
};

bus_space_t mace_pcibbus_io_tag = {
	NULL,
	(bus_addr_t)MACE_PCI_IO_BASE,
	NULL,
	0,
	mace_pcib_read_1, mace_pcib_write_1,
	mace_pcib_read_2, mace_pcib_write_2,
	mace_pcib_read_4, mace_pcib_write_4,
	mace_pcib_read_8, mace_pcib_write_8,
	mace_pcib_read_raw_2, mace_pcib_write_raw_2,
	mace_pcib_read_raw_4, mace_pcib_write_raw_4,
	mace_pcib_read_raw_8, mace_pcib_write_raw_8,
	mace_pcib_space_map, mace_pcib_space_unmap, mace_pcib_space_region,
};

/*
 * PCI doesn't have any special needs; just use the generic versions
 * of these functions.
 */
struct machine_bus_dma_tag pci_bus_dma_tag = {
	NULL,                   /* _cookie */
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
	mace_pcibr_pa_to_device,
	mace_pcibr_device_to_pa,
	CRIME_MEMORY_MASK
};

const struct _perr_map {
	pcireg_t mask;
	pcireg_t flag;
	const char *text;
} perr_map[] = {
  { PERR_MASTER_ABORT,	PERR_MASTER_ABORT_ADDR_VALID,	"master abort" },
  { PERR_TARGET_ABORT,	PERR_TARGET_ABORT_ADDR_VALID,	"target abort" },
  { PERR_DATA_PARITY_ERR,PERR_DATA_PARITY_ADDR_VALID,	"data parity error" },
  { PERR_RETRY_ERR,	PERR_RETRY_ADDR_VALID,	"retry error" },
  { PERR_ILLEGAL_CMD,	0,	"illegal command" },
  { PERR_SYSTEM_ERR,	0,	"system error" },
  { PERR_INTERRUPT_TEST,0,	"interrupt test" },
  { PERR_PARITY_ERR,	0,	"parity error" },
  { PERR_OVERRUN,	0,	"overrun error" },
  { PERR_RSVD,		0,	"reserved ??" },
  { PERR_MEMORY_ADDR,	0,	"memory address" },
  { PERR_CONFIG_ADDR,	0,	"config address" },
  { 0, 0 }
};

static int      mace_pcibrprint(void *, const char *pnp);

int
mace_pcibrmatch(struct device *parent, void *match, void *aux)
{
	switch (sys_config.system_type) {
	case SGI_O2:
		return 1;
	default:
		return 0;
	}
}

void
mace_pcibrattach(struct device *parent, struct device *self, void *aux)
{
	struct mace_pcibr_softc *sc = (struct mace_pcibr_softc *)self;
	struct pcibus_attach_args pba;
	struct confargs *ca = aux;
	pcireg_t pcireg;

	/*
	 *  Common to all bridge chips.
	 */
	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = mace_pcibr_attach_hook;
	sc->sc_pc.pc_make_tag = mace_pcibr_make_tag;
	sc->sc_pc.pc_decompose_tag = mace_pcibr_decompose_tag;

	/* Create extents for PCI mappings */
	mace_pcibbus_io_tag.bus_extent = extent_create("pci_io",
	    MACE_PCI_IO_BASE, MACE_PCI_IO_BASE + MACE_PCI_IO_SIZE - 1,
	    M_DEVBUF, (caddr_t)pci_io_ext_storage,
	    sizeof(pci_io_ext_storage), EX_NOCOALESCE|EX_NOWAIT);

	mace_pcibbus_mem_tag.bus_extent = extent_create("pci_mem",
	    MACE_PCI_MEM_BASE, MACE_PCI_MEM_BASE + MACE_PCI_MEM_SIZE - 1,
	    M_DEVBUF, (caddr_t)pci_mem_ext_storage,
	    sizeof(pci_mem_ext_storage), EX_NOCOALESCE|EX_NOWAIT);

	/* local -> PCI MEM mapping offset */
	sc->sc_mem_bus_space = &mace_pcibbus_mem_tag;

	/* local -> PCI IO mapping offset */
	sc->sc_io_bus_space = &mace_pcibbus_io_tag;

	/* Map in PCI control registers */
	sc->sc_memt = ca->ca_memt;
	if (bus_space_map(sc->sc_memt, MACE_PCI_OFFS, 4096, 0, &sc->sc_memh)) {
		printf(": can't map PCI control registers\n");
		return;
	}
	pcireg = bus_space_read_4(sc->sc_memt, sc->sc_memh, MACE_PCI_REVISION);

	printf(": mace rev %d, host system O2\n", pcireg);

	/* Register the PCI ERROR interrupt handler */
	macebus_intr_establish(NULL, 8, IST_LEVEL, IPL_HIGH,
	    mace_pcibr_errintr, (void *)sc, sc->sc_dev.dv_xname);

	sc->sc_pc.pc_bus_maxdevs = mace_pcibr_bus_maxdevs;
	sc->sc_pc.pc_conf_read = mace_pcibr_conf_read;
	sc->sc_pc.pc_conf_write = mace_pcibr_conf_write;

	sc->sc_pc.pc_intr_v = NULL;
	sc->sc_pc.pc_intr_map = mace_pcibr_intr_map;
	sc->sc_pc.pc_intr_string = mace_pcibr_intr_string;
	sc->sc_pc.pc_intr_establish = mace_pcibr_intr_establish;
	sc->sc_pc.pc_intr_disestablish = mace_pcibr_intr_disestablish;
	sc->sc_pc.pc_ppb_setup = mace_pcibr_ppb_setup;

	/*
	 *  Firmware sucks. Remap PCI BAR registers. (sigh)
	 */
	pciaddr_remap(&sc->sc_pc);

	/*
	 * Setup any PCI-PCI bridge.
	 */
	mace_pcibr_configure(sc);

	/*
	 *  Configure our PCI devices.
	 */
	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = sc->sc_io_bus_space;
	pba.pba_memt = sc->sc_mem_bus_space;
	pba.pba_dmat = malloc(sizeof(pci_bus_dma_tag), M_DEVBUF, M_NOWAIT);
	*pba.pba_dmat = pci_bus_dma_tag;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0;
	config_found(self, &pba, mace_pcibrprint);

	/* Clear PCI errors and set up error interrupt */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, MACE_PCI_ERROR_FLAGS, 0);
	pcireg = bus_space_read_4(sc->sc_memt, sc->sc_memh, MACE_PCI_CONTROL);
	pcireg |= MACE_PCI_INTCTRL;
	bus_space_write_4(sc->sc_memt, sc->sc_memh, MACE_PCI_CONTROL, pcireg);
}

static int
mace_pcibrprint(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}

void
mace_pcibr_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

int
mace_pcibr_errintr(void *v)
{
	struct mace_pcibr_softc *sc = v;
	bus_space_tag_t memt = sc->sc_memt;
	bus_space_handle_t memh = sc->sc_memh;
	const struct _perr_map *emap = perr_map;
	pcireg_t stat, erraddr;

	/* Check and clear any PCI error, report found */
	stat = bus_space_read_4(memt, memh, MACE_PCI_ERROR_FLAGS);
	erraddr = bus_space_read_4(memt, memh, MACE_PCI_ERROR_ADDRESS);
	while (emap->mask) {
		if (stat & emap->mask) {
			printf("mace: pci err %s", emap->text);
			if (emap->flag && stat & emap->flag)
				printf(" at address %p", erraddr);
			printf("\n");
		}
		emap++;
	}
	bus_space_write_4(memt, memh, MACE_PCI_ERROR_FLAGS, 0);
	return 1;
}

/*
 *  PCI access drivers
 */

pcitag_t
mace_pcibr_make_tag(void *cpv, int bus, int dev, int fnc)
{
	return (bus << 16) | (dev << 11) | (fnc << 8);
}

void
mace_pcibr_decompose_tag(void *cpv, pcitag_t tag, int *busp, int *devp,
    int *fncp)
{
	if (busp != NULL)
		*busp = (tag >> 16) & 0xff;
	if (devp != NULL)
		*devp = (tag >> 11) & 0x1f;
	if (fncp != NULL)
		*fncp = (tag >> 8) & 0x7;
}

int
mace_pcibr_bus_maxdevs(void *cpv, int busno)
{
	return busno == 0 ? 4 : 32;
}

pcireg_t
mace_pcibr_conf_read(void *cpv, pcitag_t tag, int offset)
{
	struct mace_pcibr_softc *sc = cpv;
	bus_space_tag_t memt = sc->sc_memt;
	bus_space_handle_t memh = sc->sc_memh;
	pcireg_t data, stat;
	int bus, dev;
	int s;

	s = splhigh();

	bus_space_write_4(memt, memh, MACE_PCI_ERROR_FLAGS, 0);
	data = tag | offset;
	bus_space_write_4(memt, memh, MACE_PCI_CFGADDR, data);
	data = bus_space_read_4(memt, memh, MACE_PCI_CFGDATA);
	bus_space_write_4(memt, memh, MACE_PCI_CFGADDR, 0);

	/*
	 * Onboard ahc on O2 can do Ultra speed despite not
	 * having SEEPROM nor external precision resistors.
	 */
	mace_pcibr_decompose_tag(cpv, tag, &bus, &dev, NULL);
	if (bus == 0 && (dev == 1 || dev == 2) && offset == 0x40)
		data |= 0x1000;	/* REXTVALID */

	/* Check and clear any PCI error, returns -1 if error is found */
	stat = bus_space_read_4(memt, memh, MACE_PCI_ERROR_FLAGS);
	bus_space_write_4(memt, memh, MACE_PCI_ERROR_FLAGS, 0);
	if (stat & (PERR_MASTER_ABORT | PERR_TARGET_ABORT |
		    PERR_DATA_PARITY_ERR | PERR_RETRY_ERR)) {
		data = -1;
	}

	splx(s);
	return(data);
}

void
mace_pcibr_conf_write(void *cpv, pcitag_t tag, int offset, pcireg_t data)
{
	struct mace_pcibr_softc *sc = cpv;
	pcireg_t addr;
	int s;

	s = splhigh();

	addr = tag | offset;
	bus_space_write_4(sc->sc_memt, sc->sc_memh, MACE_PCI_CFGADDR, addr);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, MACE_PCI_CFGDATA, data);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, MACE_PCI_CFGADDR, 0);

	splx(s);
}

int
mace_pcibr_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int bus, dev, pin = pa->pa_rawintrpin;
	static const signed char intrmap[][PCI_INTERRUPT_PIN_MAX] = {
		{ -1, -1, -1, -1 },
		{ 9, -1, -1, -1 },	/* ahc0 */
		{ 10, -1, -1, -1 },	/* ahc1 */
		{ 11, 14, 15, 16 },	/* slot */
#ifdef useless
		{ 12, 16, 14, 15 },	/* no slots... */
		{ 13, 15, 16, 14 }	/* ... unless you solder them */
#endif
	};

	*ihp = -1;

	if (pin == 0) {
		/* No IRQ used. */
		return 1;
	}
#ifdef DIAGNOSTIC
	if (pin > PCI_INTERRUPT_PIN_MAX) {
		printf("mace_pcibr_intr_map: bad interrupt pin %d\n", pin);
		return 1;
	}
#endif

	pci_decompose_tag(pa->pa_pc, pa->pa_tag, &bus, &dev, NULL);

	if (pa->pa_bridgetag) {
		pin = PPB_INTERRUPT_SWIZZLE(pin, dev);
		*ihp = pa->pa_bridgeih[pin - PCI_INTERRUPT_PIN_A];

		return ((*ihp == -1) ? 1 : 0);
	}

	if (dev < nitems(intrmap))
		*ihp = intrmap[dev][pin - PCI_INTERRUPT_PIN_A];

	return ((*ihp == -1) ? 1 : 0);
}

const char *
mace_pcibr_intr_string(void *lcv, pci_intr_handle_t ih)
{
	static char str[16];

	snprintf(str, sizeof(str), "irq %d", ih);
	return(str);
}

void *
mace_pcibr_intr_establish(void *lcv, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, char *name)
{
	return
	    macebus_intr_establish(NULL, ih, IST_LEVEL, level, func, arg, name);
}

void
mace_pcibr_intr_disestablish(void *lcv, void *cookie)
{
	macebus_intr_disestablish(lcv, cookie);
}

/*
 *  Bus access primitives
 */

u_int8_t
mace_pcib_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int8_t *)(h + (o ^ 3));
}

u_int16_t
mace_pcib_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int16_t *)(h + (o ^ 2));
}

u_int32_t
mace_pcib_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int32_t *)(h + o);
}

u_int64_t
mace_pcib_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int64_t *)(h + o);
}

void
mace_pcib_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int8_t v)
{
	*(volatile u_int8_t *)(h + (o ^ 3)) = v;
}

void
mace_pcib_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int16_t v)
{
	*(volatile u_int16_t *)(h + (o ^ 2)) = v;
}

void
mace_pcib_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int32_t v)
{
	*(volatile u_int32_t *)(h + o) = v;
}

void
mace_pcib_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int64_t v)
{
	*(volatile u_int64_t *)(h + o) = v;
}

void
mace_pcib_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	volatile u_int16_t *addr = (volatile u_int16_t *)(h + (o ^ 2));
	len >>= 1;
	while (len-- != 0) {
		*(u_int16_t *)buf = letoh16(*addr);
		buf += 2;
	}
}

void
mace_pcib_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	volatile u_int16_t *addr = (volatile u_int16_t *)(h + (o ^ 2));
	len >>= 1;
	while (len-- != 0) {
		*addr = htole16(*(u_int16_t *)buf);
		buf += 2;
	}
}

void
mace_pcib_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	volatile u_int32_t *addr = (volatile u_int32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*(u_int32_t *)buf = letoh32(*addr);
		buf += 4;
	}
}

void
mace_pcib_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	volatile u_int32_t *addr = (volatile u_int32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*addr = htole32(*(u_int32_t *)buf);
		buf += 4;
	}
}

void
mace_pcib_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	volatile u_int64_t *addr = (volatile u_int64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*(u_int64_t *)buf = letoh64(*addr);
		buf += 8;
	}
}

void
mace_pcib_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	volatile u_int64_t *addr = (volatile u_int64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*addr = htole64(*(u_int64_t *)buf);
		buf += 8;
	}
}

extern int extent_malloc_flags;

int
mace_pcib_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int cacheable, bus_space_handle_t *bshp)
{
	bus_addr_t bpa;
	int error;

	bpa = t->bus_base + (offs & 0x7fffffff);

	if ((error = extent_alloc_region(t->bus_extent, bpa, size,
	    EX_NOWAIT | extent_malloc_flags))) {
		return error;
	}

	if ((error  = bus_mem_add_mapping(bpa, size, cacheable, bshp))) {
		if (extent_free(t->bus_extent, bpa, size,
		    EX_NOWAIT | extent_malloc_flags)) {
			printf("bus_space_map: pa %p, size %p\n", bpa, size);
			printf("bus_space_map: can't free region\n");
		}
	}

	return (error);
}

void
mace_pcib_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t size)
{
	bus_addr_t sva;
	bus_size_t off, len;
	bus_addr_t paddr;

	/* should this verify that the proper size is freed? */
	sva = trunc_page(bsh);
	off = bsh - sva;
	len = size+off;

	if (pmap_extract(pmap_kernel(), bsh, (void *)&paddr) == 0) {
		printf("bus_space_unmap: no pa for va %p\n", bsh);
		return;
	}

	if (extent_free(t->bus_extent, paddr, size,
	    EX_NOWAIT | extent_malloc_flags)) {
		printf("bus_space_map: pa %p, size %p\n", paddr, size);
		printf("bus_space_map: can't free region\n");
	}
}

int
mace_pcib_space_region(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

/*
 * Mace PCI bus_dma helpers.
 * The PCI bus accesses memory contiguously at 0x00000000 onwards.
 */

bus_addr_t
mace_pcibr_pa_to_device(paddr_t pa)
{
	return (pa & CRIME_MEMORY_MASK);
}

paddr_t
mace_pcibr_device_to_pa(bus_addr_t addr)
{
	paddr_t pa = (paddr_t)addr & CRIME_MEMORY_MASK;

	if (pa >= 256 * 1024 * 1024)
		pa |= CRIME_MEMORY_OFFSET;

	return (pa);
}

/*
 * PCI configuration.
 */

void
mace_pcibr_configure(struct mace_pcibr_softc *sc)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	int dev;
	uint curppb, nppb;
	pcitag_t tag;
	pcireg_t id, bhlcr;

	nppb = 0;
	for (dev = 0; dev < pci_bus_maxdevs(pc, 0); dev++) {
		tag = pci_make_tag(pc, 0, dev, 0);

		id = pci_conf_read(pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlcr) == 1)
			nppb++;
	}

	/*
	 * Since there is only one working slot, there should be only
	 * up to one bridge, which we'll map after the on-board device
	 * resources.
	 */
	if (nppb != 1)
		return;

	curppb = 0;
	for (dev = 0; dev < pci_bus_maxdevs(pc, 0); dev++) {
		tag = pci_make_tag(pc, 0, dev, 0);

		id = pci_conf_read(pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlcr) != 1)
			continue;

		ppb_initialize(pc, tag, 1 + curppb * (255 / nppb),
		    (curppb + 1) * (255 / nppb));
		curppb++;
	}
}

int
mace_pcibr_ppb_setup(void *cookie, pcitag_t tag, bus_addr_t *iostart,
    bus_addr_t *ioend, bus_addr_t *memstart, bus_addr_t *memend)
{
	if (*memend != 0) {
		/*
		 * Give all resources to the bridge.
		 */
		*memstart = 0x90000000;
		*memend = 0xffffffff;
	} else {
		*memstart = 0xffffffff;
		*memend = 0;
	}

	if (*ioend != 0) {
		/*
		 * Give all resources to the bridge.
		 */
		*iostart = 0x01000000;
		*ioend = MACE_PCI_IO_SIZE - *iostart;
	} else {
		*iostart = 0xffffffff;
		*ioend = 0;
	}

	return 0;
}
