/*	$OpenBSD: macepcibridge.c,v 1.49 2018/12/03 13:46:30 visa Exp $ */

/*
 * Copyright (c) 2009 Miodrag Vallat.
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

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/vmparam.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/ppbreg.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/rbus.h>

#include <mips64/archtype.h>
#include <sgi/localbus/crimebus.h>
#include <sgi/localbus/macebus.h>
#include <sgi/localbus/macebusvar.h>
#include <sgi/pci/macepcibrvar.h>

#include "cardbus.h"

int	 mace_pcibrmatch(struct device *, void *, void *);
void	 mace_pcibrattach(struct device *, struct device *, void *);

void	 mace_pcibr_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	 mace_pcibr_bus_maxdevs(void *, int);
pcitag_t mace_pcibr_make_tag(void *, int, int, int);
void	 mace_pcibr_decompose_tag(void *, pcitag_t, int *, int *, int *);
int	 mace_pcibr_conf_size(void *, pcitag_t);
pcireg_t mace_pcibr_conf_read(void *, pcitag_t, int);
void	 mace_pcibr_conf_write(void *, pcitag_t, int, pcireg_t);
int	 mace_pcibr_probe_device_hook(void *, struct pci_attach_args *);
int	 mace_pcibr_get_widget(void *);
int	 mace_pcibr_get_dl(void *, pcitag_t, struct sgi_device_location *);
int	 mace_pcibr_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *mace_pcibr_intr_string(void *, pci_intr_handle_t);
void	*mace_pcibr_intr_establish(void *, pci_intr_handle_t, int,
	    int (*)(void *), void *, const char *);
void	 mace_pcibr_intr_disestablish(void *, void *);
int	 mace_pcibr_intr_line(void *, pci_intr_handle_t);
int	 mace_pcibr_ppb_setup(void *, pcitag_t, bus_addr_t *, bus_addr_t *,
	    bus_addr_t *, bus_addr_t *);
void	*mace_pcibr_rbus_parent_io(struct pci_attach_args *);
void	*mace_pcibr_rbus_parent_mem(struct pci_attach_args *);

void	*mace_pcib_space_vaddr(bus_space_tag_t, bus_space_handle_t);
void	 mace_pcib_space_barrier(bus_space_tag_t, bus_space_handle_t,
	    bus_size_t, bus_size_t, int);
bus_addr_t mace_pcibr_pa_to_device(paddr_t, int);

int	mace_pcibr_rbus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t,
	    int, bus_space_handle_t *);
void	mace_pcibr_rbus_space_unmap(bus_space_tag_t, bus_space_handle_t,
	    bus_size_t, bus_addr_t *);

void	mace_pcibr_configure(struct mace_pcibr_softc *);
void	mace_pcibr_device_fixup(struct mace_pcibr_softc *, int, int);
int	mace_pcibr_errintr(void *);

struct cfattach macepcibr_ca = {
	sizeof(struct mace_pcibr_softc), mace_pcibrmatch, mace_pcibrattach,
};

struct cfdriver macepcibr_cd = {
	NULL, "macepcibr", DV_DULL,
};

bus_space_t mace_pcibbus_mem_tag = {
	PHYS_TO_XKPHYS(MACE_PCI_MEM_BASE, CCA_NC),
	NULL,
	mace_pcib_read_1, mace_pcib_write_1,
	mace_pcib_read_2, mace_pcib_write_2,
	mace_pcib_read_4, mace_pcib_write_4,
	mace_pcib_read_8, mace_pcib_write_8,
	mace_pcib_read_raw_2, mace_pcib_write_raw_2,
	mace_pcib_read_raw_4, mace_pcib_write_raw_4,
	mace_pcib_read_raw_8, mace_pcib_write_raw_8,
	mace_pcib_space_map, mace_pcib_space_unmap, mace_pcib_space_region,
	mace_pcib_space_vaddr, mace_pcib_space_barrier
};

bus_space_t mace_pcibbus_io_tag = {
	PHYS_TO_XKPHYS(MACE_PCI_IO_BASE, CCA_NC),
	NULL,
	mace_pcib_read_1, mace_pcib_write_1,
	mace_pcib_read_2, mace_pcib_write_2,
	mace_pcib_read_4, mace_pcib_write_4,
	mace_pcib_read_8, mace_pcib_write_8,
	mace_pcib_read_raw_2, mace_pcib_write_raw_2,
	mace_pcib_read_raw_4, mace_pcib_write_raw_4,
	mace_pcib_read_raw_8, mace_pcib_write_raw_8,
	mace_pcib_space_map, mace_pcib_space_unmap, mace_pcib_space_region,
	mace_pcib_space_vaddr, mace_pcib_space_barrier
};

static const struct mips_pci_chipset mace_pci_chipset = {
	.pc_attach_hook = mace_pcibr_attach_hook,
	.pc_bus_maxdevs = mace_pcibr_bus_maxdevs,
	.pc_make_tag = mace_pcibr_make_tag,
	.pc_decompose_tag = mace_pcibr_decompose_tag,
	.pc_conf_size = mace_pcibr_conf_size,
	.pc_conf_read = mace_pcibr_conf_read,
	.pc_conf_write = mace_pcibr_conf_write,
	.pc_probe_device_hook = mace_pcibr_probe_device_hook,
	.pc_get_widget = mace_pcibr_get_widget,
	.pc_get_dl = mace_pcibr_get_dl,
	.pc_intr_map = mace_pcibr_intr_map,
	.pc_intr_string = mace_pcibr_intr_string,
	.pc_intr_establish = mace_pcibr_intr_establish,
	.pc_intr_disestablish = mace_pcibr_intr_disestablish,
	.pc_intr_line = mace_pcibr_intr_line,
	.pc_ppb_setup = mace_pcibr_ppb_setup,
#if NCARDBUS > 0
	.pc_rbus_parent_io = mace_pcibr_rbus_parent_io,
	.pc_rbus_parent_mem = mace_pcibr_rbus_parent_mem
#endif
};

/*
 * PCI doesn't have any special needs; just use the generic versions
 * of these functions.
 */
struct machine_bus_dma_tag mace_pci_bus_dma_tag = {
	NULL,                   /* _cookie */
	_dmamap_create,
	_dmamap_destroy,
	_dmamap_load,
	_dmamap_load_mbuf,
	_dmamap_load_uio,
	_dmamap_load_raw,
	_dmamap_load_buffer,
	_dmamap_unload,
	_dmamap_sync,
	_dmamem_alloc,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap,
	mace_pcibr_pa_to_device,
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
  { 0, 0, NULL }
};

static int mace_pcibrprint(void *, const char *pnp);

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
	struct macebus_attach_args *maa = aux;
	pcireg_t pcireg;

	sc->sc_mem_bus_space = &mace_pcibbus_mem_tag;
	sc->sc_io_bus_space = &mace_pcibbus_io_tag;

	/* Map in PCI control registers */
	sc->sc_memt = maa->maa_memt;
	if (bus_space_map(sc->sc_memt, maa->maa_baseaddr, 4096, 0,
	    &sc->sc_memh)) {
		printf(": can't map PCI control registers\n");
		return;
	}
	pcireg = bus_space_read_4(sc->sc_memt, sc->sc_memh, MACE_PCI_REVISION);

	printf(": mace rev %d\n", pcireg);

	/* Register the PCI ERROR interrupt handler */
	macebus_intr_establish(maa->maa_intr, maa->maa_mace_intr,
	    IPL_HIGH, mace_pcibr_errintr, sc, sc->sc_dev.dv_xname);

	bcopy(&mace_pci_chipset, &sc->sc_pc, sizeof(mace_pci_chipset));
	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_intr_v = NULL;

	/*
	 * The O2 firmware sucks.  It makes a mess of I/O BARs and
	 * an even bigger mess for PCI-PCI bridges.
	 */
	mace_pcibr_configure(sc);

	/*
	 *  Configure our PCI devices.
	 */
	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = sc->sc_io_bus_space;
	pba.pba_memt = sc->sc_mem_bus_space;
	pba.pba_dmat = &mace_pci_bus_dma_tag;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_ioex = extent_create("mace_io", 0, 0xffffffff, M_DEVBUF,
	    NULL, 0, EX_NOWAIT | EX_FILLED);
	if (pba.pba_ioex != NULL) {
		/*
		 * I/O accesses at address zero cause PCI errors, so
		 * make sure the first few bytes are not available.
		 */
		extent_free(pba.pba_ioex, 0x20, (1UL << 32) - 0x20, EX_NOWAIT);
	}
	pba.pba_memex = extent_create("mace_mem", 0, 0xffffffff, M_DEVBUF,
	    NULL, 0, EX_NOWAIT | EX_FILLED);
	if (pba.pba_memex != NULL)
		extent_free(pba.pba_memex, MACE_PCI_MEM_OFFSET,
		    MACE_PCI_MEM_SIZE, EX_NOWAIT);
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
				printf(" at address 0x%08x", erraddr);
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
	return busno == 0 ? 6 : 32;
}

int
mace_pcibr_conf_size(void *cpv, pcitag_t tag)
{
	return PCI_CONFIG_SPACE_SIZE;
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
mace_pcibr_probe_device_hook(void *unused, struct pci_attach_args *notused)
{
	return 0;
}

int
mace_pcibr_get_widget(void *unused)
{
	return 0;
}

int
mace_pcibr_get_dl(void *cpv, pcitag_t tag, struct sgi_device_location *sdl)
{
	int bus, device, fn;

	memset(sdl, 0, sizeof *sdl);
	mace_pcibr_decompose_tag(cpv, tag, &bus, &device, &fn);
	if (bus != 0)
		return 0;
	sdl->device = device;
	sdl->fn = fn;
	return 1;
}

int
mace_pcibr_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int bus, dev, pin = pa->pa_rawintrpin;
	static const signed char intrmap[][PCI_INTERRUPT_PIN_MAX] = {
		{ -1, -1, -1, -1 },
		{ 8, -1, -1, -1 },	/* ahc0 */
		{ 9, -1, -1, -1 },	/* ahc1 */
		{ 10, 13, 14, 15 },	/* slot */
		{ 11, 15, 13, 14 },	/* no slots... */
		{ 12, 14, 15, 13 }	/* ... unless you solder them */
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

	snprintf(str, sizeof(str), "irq %ld", ih);
	return(str);
}

void *
mace_pcibr_intr_establish(void *lcv, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, const char *name)
{
	return macebus_intr_establish(ih, 0, level, func, arg, name);
}

void
mace_pcibr_intr_disestablish(void *lcv, void *ih)
{
	macebus_intr_disestablish(ih);
}

int
mace_pcibr_intr_line(void *lcv, pci_intr_handle_t ih)
{
	return ih;
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

int
mace_pcib_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
#ifdef DIAGNOSTIC
	if (t->bus_base == mace_pcibbus_mem_tag.bus_base) {
		if (offs < MACE_PCI_MEM_OFFSET ||
		    ((offs + size - 1) >> 32) != 0)
			return EINVAL;
	} else {
		if (((offs + size - 1) >> 32) != 0)
			return EINVAL;
	}
#endif

	if (ISSET(flags, BUS_SPACE_MAP_CACHEABLE))
		offs +=
		    PHYS_TO_XKPHYS(0, CCA_CACHED) - PHYS_TO_XKPHYS(0, CCA_NC);
	*bshp = t->bus_base + offs;
	return 0;
}

void
mace_pcib_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t size)
{
}

int
mace_pcib_space_region(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void *
mace_pcib_space_vaddr(bus_space_tag_t t, bus_space_handle_t h)
{
	return (void *)h;
}

void
mace_pcib_space_barrier(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t offs, bus_size_t len, int flags)
{
	mips_sync();
}

/*
 * Mace PCI bus_dma helpers.
 * The PCI bus accesses memory contiguously at 0x00000000 onwards.
 */

bus_addr_t
mace_pcibr_pa_to_device(paddr_t pa, int flags)
{
	return (pa & CRIME_MEMORY_MASK);
}

/*
 * PCI configuration.
 */

void
mace_pcibr_configure(struct mace_pcibr_softc *sc)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	pcitag_t tag;
	pcireg_t id, bhlcr;
	int dev, nfuncs;
	uint nppb, npccbb;
	const struct pci_quirkdata *qd;

	nppb = npccbb = 0;
	for (dev = 0; dev < pci_bus_maxdevs(pc, 0); dev++) {
		tag = pci_make_tag(pc, 0, dev, 0);

		id = pci_conf_read(pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlcr) == 1)
			nppb++;
		if (PCI_HDRTYPE_TYPE(bhlcr) == 2)
			npccbb++;

		qd = pci_lookup_quirkdata(PCI_VENDOR(id), PCI_PRODUCT(id));
		if (PCI_HDRTYPE_MULTIFN(bhlcr) ||
		    (qd != NULL && (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0))
			nfuncs = 8;
		else
			nfuncs = 1;

		mace_pcibr_device_fixup(sc, dev, nfuncs);
	}

	/*
	 * Since there is only one working slot, there should be only
	 * up to one bridge (PCI-PCI or PCI-CardBus), which we'll map
	 * after the on-board device resources.
	 */
	if (nppb + npccbb != 1)
		return;

	for (dev = 0; dev < pci_bus_maxdevs(pc, 0); dev++) {
		tag = pci_make_tag(pc, 0, dev, 0);

		id = pci_conf_read(pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		switch (PCI_HDRTYPE_TYPE(bhlcr)) {
		case 1:
			ppb_initialize(pc, tag, 0, 1, 255);
			break;
		case 2:
			pccbb_initialize(pc, tag, 0, 1, 1);
			break;
		}
	}
}

void
mace_pcibr_device_fixup(struct mace_pcibr_softc *sc, int dev, int nfuncs)
{
	pci_chipset_tag_t pc = &sc->sc_pc;
	pcitag_t tag;
	pcireg_t csr, bhlcr, type;
	int function;
	int reg, reg_start, reg_end;

	for (function = 0; function < nfuncs; function++) {
		tag = pci_make_tag(pc, 0, dev, function);

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		switch (PCI_HDRTYPE_TYPE(bhlcr)) {
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
			continue;
		}

		/*
		 * The firmware will only initialize memory BARs, and only
		 * the lower half of them if they are 64 bit.
		 * So here we disable I/O space and reset the I/O BARs to 0,
		 * and make sure the upper part of 64 bit memory BARs is
		 * correct.
		 * Device drivers will allocate resources themselves and
		 * enable I/O space on an as-needed basis.
		 */
		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
	            csr & ~PCI_COMMAND_IO_ENABLE);

		for (reg = reg_start; reg < reg_end; reg += 4) {
			if (pci_mapreg_probe(pc, tag, reg, &type) == 0)
				continue;

			switch (type) {
			case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
				reg += 4;
				/* FALLTHROUGH */
			case PCI_MAPREG_TYPE_IO:
				pci_conf_write(pc, tag, reg, 0);
				break;
			}
		}
	}
}

int
mace_pcibr_ppb_setup(void *cookie, pcitag_t tag, bus_addr_t *iostart,
    bus_addr_t *ioend, bus_addr_t *memstart, bus_addr_t *memend)
{
	if (*memend != 0) {
		/*
		 * Give all resources to the bridge
		 * (except for the few the on-board ahc(4) will use).
		 */
		*memstart = 0x81000000;
		*memend =   0xffffffff;
	} else {
		*memstart = 0xffffffff;
		*memend = 0;
	}

	if (*ioend != 0) {
		/*
		 * Give all resources to the bridge
		 * (except for the few the on-board ahc(4) will use).
		 */
		*iostart = 0x00010000;
		*ioend =   0xffffffff;
	} else {
		*iostart = 0xffffffff;
		*ioend = 0;
	}

	return 0;
}

#if NCARDBUS > 0

static struct rb_md_fnptr mace_pcibr_rb_md_fn = {
	mace_pcibr_rbus_space_map,
	mace_pcibr_rbus_space_unmap
};

int
mace_pcibr_rbus_space_map(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	return bus_space_map(t, addr, size, flags, bshp);
}

void
mace_pcibr_rbus_space_unmap(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t size, bus_addr_t *addrp)
{
	bus_space_unmap(t, h, size);
	/* can't simply subtract because of possible cacheability */
	*addrp = XKPHYS_TO_PHYS(h) - XKPHYS_TO_PHYS(t->bus_base);
}

void *
mace_pcibr_rbus_parent_io(struct pci_attach_args *pa)
{
	rbus_tag_t rb;

	rb = rbus_new_root_share(pa->pa_iot, pa->pa_ioex,
	    0x0000, 0xffff);
	if (rb != NULL)
		rb->rb_md = &mace_pcibr_rb_md_fn;

	return rb;
}

void *
mace_pcibr_rbus_parent_mem(struct pci_attach_args *pa)
{
	rbus_tag_t rb;

	rb = rbus_new_root_share(pa->pa_memt, pa->pa_memex,
	    0, 0xffffffff);
	if (rb != NULL)
		rb->rb_md = &mace_pcibr_rb_md_fn;

	return rb;
}

#endif	/* NCARDBUS > 0 */
