/*	$OpenBSD: macepcibridge.c,v 1.4 2004/09/20 10:31:16 pefo Exp $ */

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
 * Machine dependent PCI BUS Bridge driver.
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
#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/vmparam.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <mips64/archtype.h>
#include <sgi/localbus/macebus.h>
#include <sgi/pci/macepcibrvar.h>

extern void *macebus_intr_establish(void *, u_long, int, int,
		int (*)(void *), void *, char *);
extern void macebus_intr_disestablish(void *, void *);
extern void pciaddr_remap(pci_chipset_tag_t);

/**/
int	 pcibrmatch(struct device *, void *, void *);
void	 pcibrattach(struct device *, struct device *, void *);
void	 pcibr_attach_hook(struct device *, struct device *,
				struct pcibus_attach_args *);
int	 pcibr_errintr(void *);

pcitag_t pcibr_make_tag(void *, int, int, int);
void	 pcibr_decompose_tag(void *, pcitag_t, int *, int *, int *);

int	 pcibr_bus_maxdevs(void *, int);
pcireg_t pcibr_conf_read(void *, pcitag_t, int);
void	 pcibr_conf_write(void *, pcitag_t, int, pcireg_t);

int      pcibr_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *pcibr_intr_string(void *, pci_intr_handle_t);
void     *pcibr_intr_establish(void *, pci_intr_handle_t,
	    int, int (*func)(void *), void *, char *);
void     pcibr_intr_disestablish(void *, void *);


struct cfattach macepcibr_ca = {
	sizeof(struct pcibr_softc), pcibrmatch, pcibrattach,
};

struct cfdriver macepcibr_cd = {
	NULL, "macepcibr", DV_DULL,
};

long pci_io_ext_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof (long)];
long pci_mem_ext_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof (long)];

bus_space_t pcibbus_mem_tag = {
	NULL,
	(bus_addr_t)MACE_PCI_MEM_BASE,
	NULL,
	0,
	pcib_read_1, pcib_write_1,
	pcib_read_2, pcib_write_2,
	pcib_read_4, pcib_write_4,
	pcib_read_8, pcib_write_8,
	pcib_space_map, pcib_space_unmap, pcib_space_region,
};

bus_space_t pcibbus_io_tag = {
	NULL,
	(bus_addr_t)MACE_PCI_IO_BASE,
	NULL,
	0,
	pcib_read_1, pcib_write_1,
	pcib_read_2, pcib_write_2,
	pcib_read_4, pcib_write_4,
	pcib_read_8, pcib_write_8,
	pcib_space_map, pcib_space_unmap, pcib_space_region,
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
	NULL
};

struct _perr_map {
	pcireg_t mask;
	pcireg_t flag;
	char *text;
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


static int      pcibrprint __P((void *, const char *pnp));


int
pcibrmatch(struct device *parent, void *match, void *aux)
{
	switch(sys_config.system_type) {
	case SGI_O2:
		return 1;
	}
	return (0);
}

void
pcibrattach(struct device *parent, struct device *self, void *aux)
{
	struct pcibr_softc *sc = (struct pcibr_softc *)self;
	struct pcibus_attach_args pba;
	struct confargs *ca = aux;
	pcireg_t pcireg;

	/*
	 *  Common to all bridge chips.
	 */
	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = pcibr_attach_hook;
	sc->sc_pc.pc_make_tag = pcibr_make_tag;
	sc->sc_pc.pc_decompose_tag = pcibr_decompose_tag;
	sc->sc_pc.pc_sync_cache = sys_config._IOSyncDCache;

	/* Create extents for PCI mappings */
	pcibbus_io_tag.bus_extent = extent_create("pci_io",
		MACE_PCI_IO_BASE, MACE_PCI_IO_BASE + MACE_PCI_IO_SIZE - 1,
		M_DEVBUF, (caddr_t)pci_io_ext_storage,
		sizeof(pci_io_ext_storage), EX_NOCOALESCE|EX_NOWAIT);

	pcibbus_mem_tag.bus_extent = extent_create("pci_mem",
		MACE_PCI_MEM_BASE, MACE_PCI_MEM_BASE + MACE_PCI_MEM_SIZE - 1,
		M_DEVBUF, (caddr_t)pci_mem_ext_storage,
		sizeof(pci_mem_ext_storage), EX_NOCOALESCE|EX_NOWAIT);

	/* local -> PCI MEM mapping offset */
	sc->sc_mem_bus_space = &pcibbus_mem_tag;

	/* local -> PCI IO mapping offset */
	sc->sc_io_bus_space = &pcibbus_io_tag;

	/* Map in PCI control registers */
	sc->sc_memt = ca->ca_memt;
	if (bus_space_map(sc->sc_memt, MACE_PCI_OFFS, 4096, 0, &sc->sc_memh)) {
		printf("UH-OH! Can't map PCI control registers!\n");
		return;
	}
	pcireg = bus_space_read_4(sc->sc_memt, sc->sc_memh, MACE_PCI_REVISION);

	printf(": mace rev %d, host system O2\n", pcireg);

	/* Register the PCI ERROR interrupt handler */
	BUS_INTR_ESTABLISH(ca, NULL, 8, IST_LEVEL, IPL_HIGH,
	    pcibr_errintr, (void *)sc, sc->sc_dev.dv_xname);

	sc->sc_pc.pc_bus_maxdevs = pcibr_bus_maxdevs;
	sc->sc_pc.pc_conf_read = pcibr_conf_read;
	sc->sc_pc.pc_conf_write = pcibr_conf_write;

	sc->sc_pc.pc_intr_v = NULL;
	sc->sc_pc.pc_intr_map = pcibr_intr_map;
	sc->sc_pc.pc_intr_string = pcibr_intr_string;
	sc->sc_pc.pc_intr_establish = pcibr_intr_establish;
	sc->sc_pc.pc_intr_disestablish = pcibr_intr_disestablish;

	/*
	 *  Firmware sucks. Remap PCI BAR registers. (sigh)
	 */
	pciaddr_remap(&sc->sc_pc);

	/*
	 *  Configure our PCI devices.
	 */
	pba.pba_busname = "pci";
	pba.pba_iot = sc->sc_io_bus_space;
	pba.pba_memt = sc->sc_mem_bus_space;
	pba.pba_dmat = malloc(sizeof(pci_bus_dma_tag), M_DEVBUF, M_NOWAIT);
	pci_bus_dma_tag.dma_offs = 0x00000000;
	*pba.pba_dmat = pci_bus_dma_tag;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_bus = sc->sc_dev.dv_unit;
	config_found(self, &pba, pcibrprint);

	/* Clear PCI errors and set up error interrupt */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, MACE_PCI_ERROR_FLAGS, 0);
	pcireg = bus_space_read_4(sc->sc_memt, sc->sc_memh, MACE_PCI_CONTROL);
	pcireg |= MACE_PCI_INTCTRL;
	bus_space_write_4(sc->sc_memt, sc->sc_memh, MACE_PCI_CONTROL, pcireg);
}

static int
pcibrprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return(UNCONF);
}

void
pcibr_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{
}

int
pcibr_errintr(void *v)
{
	struct pcibr_softc *sc = v;
	bus_space_tag_t memt = sc->sc_memt;
	bus_space_handle_t memh = sc->sc_memh;
	struct _perr_map *emap = perr_map;
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
pcibr_make_tag(cpv, bus, dev, fnc)
	void *cpv;
	int bus, dev, fnc;
{
	return (bus << 16) | (dev << 11) | (fnc << 8);
}

void
pcibr_decompose_tag(cpv, tag, busp, devp, fncp)
	void *cpv;
	pcitag_t tag;
	int *busp, *devp, *fncp;
{
	if (busp != NULL)
		*busp = (tag >> 16) & 0x7;
	if (devp != NULL)
		*devp = (tag >> 11) & 0x1f;
	if (fncp != NULL)
		*fncp = (tag >> 8) & 0x7;
}

int
pcibr_bus_maxdevs(cpv, busno)
	void *cpv;
	int busno;
{
	return(16);
}

pcireg_t
pcibr_conf_read(cpv, tag, offset)
	void *cpv;
	pcitag_t tag;
	int offset;
{
	struct pcibr_softc *sc = cpv;
	bus_space_tag_t memt = sc->sc_memt;
	bus_space_handle_t memh = sc->sc_memh;
	pcireg_t data, stat;
	int s;

	s = splhigh();

	bus_space_write_4(memt, memh, MACE_PCI_ERROR_FLAGS, 0);
	data = tag | offset;
	bus_space_write_4(memt, memh, MACE_PCI_CFGADDR, data);
	data = bus_space_read_4(memt, memh, MACE_PCI_CFGDATA);
	bus_space_write_4(memt, memh, MACE_PCI_CFGADDR, 0);

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
pcibr_conf_write(cpv, tag, offset, data)
	void *cpv;
	pcitag_t tag;
	int offset;
	pcireg_t data;
{
	struct pcibr_softc *sc = cpv;
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
pcibr_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int bus, device, pirq;

	*ihp = -1;

	if (pa->pa_intrpin == 0) {
		/* No IRQ used. */
		return 1;
	}
	if (pa->pa_intrpin > 4) {
		printf("pcibr_intr_map: bad interrupt pin %d\n", pa->pa_intrpin);
		return 1;
	}

	pcibr_decompose_tag((void *)NULL, pa->pa_tag, &bus, &device, NULL);

	if (sys_config.system_type == SGI_O2) {
		pirq = -1;
		switch (device) {
		case 1:
			pirq = 9;
			break;
		case 2:
			pirq = 10;
			break;
		case 3:
			pirq = 11;
			break;
		}
	}

	*ihp = pirq;
	return 0;
}

const char *
pcibr_intr_string(lcv, ih)
	void *lcv;
	pci_intr_handle_t ih;
{
static char str[16];

	snprintf(str, sizeof(str), "irq %d", ih);
	return(str);
}

void *
pcibr_intr_establish(lcv, ih, level, func, arg, name)
	void *lcv;
	pci_intr_handle_t ih;
	int level;
	int (*func) __P((void *));
	void *arg;
	char *name;
{
	return macebus_intr_establish(NULL, ih, IST_LEVEL, level, func, arg, name);
}

void
pcibr_intr_disestablish(lcv, cookie)
	void *lcv, *cookie;
{
	macebus_intr_disestablish(lcv, cookie);
}

/*
 *  Bus access primitives
 *  XXX 64 bit access not clean in lp32 mode.
 */

u_int8_t
pcib_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int8_t *)(h + (o | 3) - (o & 3));
}

u_int16_t
pcib_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int16_t *)(h + (o | 2) - (o & 3));
}

u_int32_t
pcib_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int32_t *)(h + o);
}

u_int64_t
pcib_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int64_t *)(h + o);
}

void
pcib_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{
	*(volatile u_int8_t *)(h + (o | 3) - (o & 3)) = v;
}

void
pcib_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int16_t v)
{
	*(volatile u_int16_t *)(h + (o | 2) - (o & 3)) = v;
}

void
pcib_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int32_t v)
{
	*(volatile u_int32_t *)(h + o) = v;
}

void
pcib_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int64_t v)
{
	*(volatile u_int64_t *)(h + o) = v;
}

int
pcib_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
	int cacheable, bus_space_handle_t *bshp)
{
	bus_addr_t bpa;
	int error;
	bpa = t->bus_base + (offs & 0x01ffffff);

	if ((error = extent_alloc_region(t->bus_extent, bpa, size,
	    EX_NOWAIT | EX_MALLOCOK))) {
		return error;
	}

	if ((error  = bus_mem_add_mapping(bpa, size, cacheable, bshp))) {
		if (extent_free(t->bus_extent, bpa, size, EX_NOWAIT |
		    ((phys_map != NULL) ? EX_MALLOCOK : 0))) {
			printf("bus_space_map: pa %p, size %p\n", bpa, size);
			printf("bus_space_map: can't free region\n");
		}
	}
	return 0;
}

void
pcib_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
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

	if (phys_map != NULL &&
	    ((sva >= VM_MIN_KERNEL_ADDRESS) && (sva < VM_MAX_KERNEL_ADDRESS))) {
		/* do not free memory which was stolen from the vm system */
		uvm_km_free(kernel_map, sva, len);
	}

	if (extent_free(t->bus_extent, paddr, size, EX_NOWAIT |
	    ((phys_map != NULL) ? EX_MALLOCOK : 0))) {
		printf("bus_space_map: pa %p, size %p\n", paddr, size);
		printf("bus_space_map: can't free region\n");
	}
}

int
pcib_space_region(bus_space_tag_t t, bus_space_handle_t bsh,
	bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}
