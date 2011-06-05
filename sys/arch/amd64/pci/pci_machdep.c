/*	$OpenBSD: pci_machdep.c,v 1.48 2011/06/05 10:07:30 kettenis Exp $	*/
/*	$NetBSD: pci_machdep.c,v 1.3 2003/05/07 21:33:58 fvdl Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine-specific functions for PCI autoconfiguration.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <machine/pio.h>
#include <machine/intr.h>
#include <machine/biosvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include "ioapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#include <machine/mpbiosvar.h>
#endif

/*
 * Memory Mapped Configuration space access.
 *
 * Since mapping the whole configuration space will cost us up to
 * 256MB of kernel virtual memory, we use seperate mappings per bus.
 * The mappings are created on-demand, such that we only use kernel
 * virtual memory for busses that are actually present.
 */
bus_addr_t pci_mcfg_addr;
int pci_mcfg_min_bus, pci_mcfg_max_bus;
bus_space_tag_t pci_mcfgt = X86_BUS_SPACE_MEM;
bus_space_handle_t pci_mcfgh[256];
void pci_mcfg_map_bus(int);

struct mutex pci_conf_lock = MUTEX_INITIALIZER(IPL_HIGH);

#define	PCI_CONF_LOCK()						\
do {									\
	mtx_enter(&pci_conf_lock);					\
} while (0)

#define	PCI_CONF_UNLOCK()						\
do {									\
	mtx_leave(&pci_conf_lock);					\
} while (0)

#define	PCI_MODE1_ENABLE	0x80000000UL
#define	PCI_MODE1_ADDRESS_REG	0x0cf8
#define	PCI_MODE1_DATA_REG	0x0cfc

/*
 * PCI doesn't have any special needs; just use the generic versions
 * of these functions.
 */
struct bus_dma_tag pci_bus_dma_tag = {
	NULL,			/* _may_bounce */
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	_bus_dmamap_sync,
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

void
pci_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
	pci_chipset_tag_t pc = pba->pba_pc;
	pcitag_t tag;
	pcireg_t id, class;

	if (pba->pba_bus != 0)
		return;

	/*
	 * In order to decide whether the system supports MSI we look
	 * at the host bridge, which should be device 0 function 0 on
	 * bus 0.  It is better to not enable MSI on systems that
	 * support it than the other way around, so be conservative
	 * here.  So we don't enable MSI if we don't find a host
	 * bridge there.  We also deliberately don't enable MSI on
	 * chipsets from low-end manifacturers like VIA and SiS.
	 */
	tag = pci_make_tag(pc, 0, 0, 0);
	id = pci_conf_read(pc, tag, PCI_ID_REG);
	class = pci_conf_read(pc, tag, PCI_CLASS_REG);

	if (PCI_CLASS(class) != PCI_CLASS_BRIDGE ||
	    PCI_SUBCLASS(class) != PCI_SUBCLASS_BRIDGE_HOST)
		return;

	switch (PCI_VENDOR(id)) {
	case PCI_VENDOR_INTEL:
		/*
		 * In the land of VMWare you can have the latest
		 * 64-bit AMD multicore CPU behind a prehistoric Intel
		 * host bridge.  Give them what they deserve.
		 */
		switch (PCI_PRODUCT(id)) {
		case PCI_PRODUCT_INTEL_82443BX:
			break;
		default:
			pba->pba_flags |= PCI_FLAGS_MSI_ENABLED;
			break;
		}
		break;
	case PCI_VENDOR_NVIDIA:
	case PCI_VENDOR_AMD:
		pba->pba_flags |= PCI_FLAGS_MSI_ENABLED;
		break;
	}

	/*
	 * Don't enable MSI on a HyperTransport bus.  In order to
	 * determine that bus 0 is a HyperTransport bus, we look at
	 * device 24 function 0, which is the HyperTransport
	 * host/primary interface integrated on most 64-bit AMD CPUs.
	 * If that device has a HyperTransport capability, bus 0 must
	 * be a HyperTransport bus and we disable MSI.
	 */
	tag = pci_make_tag(pc, 0, 24, 0);
	if (pci_get_capability(pc, tag, PCI_CAP_HT, NULL, NULL))
		pba->pba_flags &= ~PCI_FLAGS_MSI_ENABLED;
}

int
pci_bus_maxdevs(pci_chipset_tag_t pc, int busno)
{
	return (32);
}

pcitag_t
pci_make_tag(pci_chipset_tag_t pc, int bus, int device, int function)
{
	if (bus >= 256 || device >= 32 || function >= 8)
		panic("pci_make_tag: bad request");

	return (PCI_MODE1_ENABLE |
	    (bus << 16) | (device << 11) | (function << 8));
}

void
pci_decompose_tag(pci_chipset_tag_t pc, pcitag_t tag, int *bp, int *dp, int *fp)
{
	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x7;
}

int
pci_conf_size(pci_chipset_tag_t pc, pcitag_t tag)
{
	int bus;

	if (pci_mcfg_addr) {
		pci_decompose_tag(pc, tag, &bus, NULL, NULL);
		if (bus >= pci_mcfg_min_bus && bus <= pci_mcfg_max_bus)
			return PCIE_CONFIG_SPACE_SIZE;
	}

	return PCI_CONFIG_SPACE_SIZE;
}

void
pci_mcfg_map_bus(int bus)
{
	if (pci_mcfgh[bus])
		return;

	if (bus_space_map(pci_mcfgt, pci_mcfg_addr + (bus << 20), 1 << 20,
	    0, &pci_mcfgh[bus]))
		panic("pci_conf_read: cannot map mcfg space");
}

pcireg_t
pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	pcireg_t data;
	int bus;

	if (pci_mcfg_addr && reg >= PCI_CONFIG_SPACE_SIZE) {
		pci_decompose_tag(pc, tag, &bus, NULL, NULL);
		if (bus >= pci_mcfg_min_bus && bus <= pci_mcfg_max_bus) {
			pci_mcfg_map_bus(bus);
			data = bus_space_read_4(pci_mcfgt, pci_mcfgh[bus],
			    (tag & 0x000ff00) << 4 | reg);
			return data;
		}
	}

	PCI_CONF_LOCK();
	outl(PCI_MODE1_ADDRESS_REG, tag | reg);
	data = inl(PCI_MODE1_DATA_REG);
	outl(PCI_MODE1_ADDRESS_REG, 0);
	PCI_CONF_UNLOCK();

	return data;
}

void
pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t data)
{
	int bus;

	if (pci_mcfg_addr && reg >= PCI_CONFIG_SPACE_SIZE) {
		pci_decompose_tag(pc, tag, &bus, NULL, NULL);
		if (bus >= pci_mcfg_min_bus && bus <= pci_mcfg_max_bus) {
			pci_mcfg_map_bus(bus);
			bus_space_write_4(pci_mcfgt, pci_mcfgh[bus],
			    (tag & 0x000ff00) << 4 | reg, data);
			return;
		}
	}

	PCI_CONF_LOCK();
	outl(PCI_MODE1_ADDRESS_REG, tag | reg);
	outl(PCI_MODE1_DATA_REG, data);
	outl(PCI_MODE1_ADDRESS_REG, 0);
	PCI_CONF_UNLOCK();
}

void msi_hwmask(struct pic *, int);
void msi_hwunmask(struct pic *, int);
void msi_setup(struct pic *, struct cpu_info *, int, int, int);

struct pic msi_pic = {
	{0, {NULL}, NULL, 0, "msi", NULL, 0, 0},
	PIC_MSI,
#ifdef MULTIPROCESSOR
	{},
#endif
	msi_hwmask,
	msi_hwunmask,
	msi_setup,
	msi_setup,
	NULL,
	ioapic_edge_stubs
};

void
msi_hwmask(struct pic *pic, int pin)
{
}

void
msi_hwunmask(struct pic *pic, int pin)
{
}

void
msi_setup(struct pic *pic, struct cpu_info *ci, int pin, int idtvec, int type)
{
}

int
pci_intr_map_msi(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;

	if ((pa->pa_flags & PCI_FLAGS_MSI_ENABLED) == 0 || mp_busses == NULL ||
	    pci_get_capability(pc, tag, PCI_CAP_MSI, NULL, NULL) == 0)
		return 1;

	ihp->tag = tag;
	ihp->line = APIC_INT_VIA_MSG;
	ihp->pin = 0;
	return 0;
}

int
pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int pin = pa->pa_rawintrpin;
	int line = pa->pa_intrline;
#if NIOAPIC > 0
	int bus, dev, func;
	int mppin;
#endif

	if (pin == 0) {
		/* No IRQ used. */
		goto bad;
	}

	if (pin > PCI_INTERRUPT_PIN_MAX) {
		printf("pci_intr_map: bad interrupt pin %d\n", pin);
		goto bad;
	}

	ihp->tag = pa->pa_tag;
	ihp->line = line;
	ihp->pin = pin;

#if NIOAPIC > 0
	pci_decompose_tag(pa->pa_pc, pa->pa_tag, &bus, &dev, &func);
	if (mp_busses != NULL) {
		mppin = (dev << 2)|(pin - 1);
		if (intr_find_mpmapping(bus, mppin, &ihp->line) == 0) {
			ihp->line |= line;
			return 0;
		}
		if (pa->pa_bridgetag) {
			int swizpin = PPB_INTERRUPT_SWIZZLE(pin, dev);
			if (pa->pa_bridgeih[swizpin - 1].line != -1) {
				ihp->line = pa->pa_bridgeih[swizpin - 1].line;
				ihp->line |= line;
				return 0;
			}
		}
		/*
		 * No explicit PCI mapping found. This is not fatal,
		 * we'll try the ISA (or possibly EISA) mappings next.
		 */
	}
#endif

	/*
	 * Section 6.2.4, `Miscellaneous Functions', says that 255 means
	 * `unknown' or `no connection' on a PC.  We assume that a device with
	 * `no connection' either doesn't have an interrupt (in which case the
	 * pin number should be 0, and would have been noticed above), or
	 * wasn't configured by the BIOS (in which case we punt, since there's
	 * no real way we can know how the interrupt lines are mapped in the
	 * hardware).
	 *
	 * XXX
	 * Since IRQ 0 is only used by the clock, and we can't actually be sure
	 * that the BIOS did its job, we also recognize that as meaning that
	 * the BIOS has not configured the device.
	 */
	if (line == 0 || line == X86_PCI_INTERRUPT_LINE_NO_CONNECTION)
		goto bad;

	if (line >= NUM_LEGACY_IRQS) {
		printf("pci_intr_map: bad interrupt line %d\n", line);
		goto bad;
	}
	if (line == 2) {
		printf("pci_intr_map: changed line 2 to line 9\n");
		line = 9;
	}

#if NIOAPIC > 0
	if (mp_busses != NULL) {
		if (mp_isa_bus != NULL &&
		    intr_find_mpmapping(mp_isa_bus->mb_idx, line, &ihp->line) == 0) {
			ihp->line |= line;
			return 0;
		}
#if NEISA > 0
		if (mp_eisa_bus != NULL &&
		    intr_find_mpmapping(mp_eisa_bus->mb_idx, line, &ihp->line) == 0) {
			ihp->line |= line;
			return 0;
		}
#endif
		printf("pci_intr_map: bus %d dev %d func %d pin %d; line %d\n",
		    bus, dev, func, pin, line);
		printf("pci_intr_map: no MP mapping found\n");
	}
#endif

	return 0;

bad:
	ihp->line = -1;
	return 1;
}

const char *
pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{
	static char irqstr[64];

	if (ih.line == 0)
		panic("pci_intr_string: bogus handle 0x%x", ih.line);

	if (ih.line & APIC_INT_VIA_MSG)
		return ("msi");

#if NIOAPIC > 0
	if (ih.line & APIC_INT_VIA_APIC)
		snprintf(irqstr, sizeof(irqstr), "apic %d int %d",
		    APIC_IRQ_APIC(ih.line), APIC_IRQ_PIN(ih.line));
	else
		snprintf(irqstr, sizeof(irqstr), "irq %d",
		    pci_intr_line(pc, ih));
#else
	snprintf(irqstr, sizeof(irqstr), "irq %d", pci_intr_line(pc, ih));
#endif
	return (irqstr);
}

#include "acpiprt.h"
#if NACPIPRT > 0
void	acpiprt_route_interrupt(int bus, int dev, int pin);
#endif

void *
pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, const char *what)
{
	int pin, irq;
	int bus, dev;
	pcitag_t tag = ih.tag;
	struct pic *pic;

	if (ih.line & APIC_INT_VIA_MSG) {
		struct intrhand *ih;
		struct intrsource *source;
		pcireg_t reg, addr;
		int off, vec;

		if (pci_get_capability(pc, tag, PCI_CAP_MSI, &off, &reg) == 0)
			panic("%s: no msi capability", __func__);

		ih = intr_establish(-1, &msi_pic, tag, IST_PULSE, level,
		    func, arg, what);
		if (ih == NULL)
			return (NULL);

		source = ih->ih_cpu->ci_isources[ih->ih_slot];
		addr = 0xfee00000UL | (ih->ih_cpu->ci_apicid << 12);
		vec = source->is_idtvec;

		if (reg & PCI_MSI_MC_C64) {
			pci_conf_write(pc, tag, off + PCI_MSI_MA, addr);
			pci_conf_write(pc, tag, off + PCI_MSI_MAU32, 0);
			pci_conf_write(pc, tag, off + PCI_MSI_MD64, vec);
		} else {
			pci_conf_write(pc, tag, off + PCI_MSI_MA, addr);
			pci_conf_write(pc, tag, off + PCI_MSI_MD32, vec);
		}
		pci_conf_write(pc, tag, off, reg | PCI_MSI_MC_MSIE);
		return (ih);
	}

	pci_decompose_tag(pc, ih.tag, &bus, &dev, NULL);
#if NACPIPRT > 0
	acpiprt_route_interrupt(bus, dev, ih.pin);
#endif

	pic = &i8259_pic;
	pin = irq = ih.line;

#if NIOAPIC > 0
	if (ih.line & APIC_INT_VIA_APIC) {
		pic = (struct pic *)ioapic_find(APIC_IRQ_APIC(ih.line));
		if (pic == NULL) {
			printf("pci_intr_establish: bad ioapic %d\n",
			    APIC_IRQ_APIC(ih.line));
			return NULL;
		}
		pin = APIC_IRQ_PIN(ih.line);
		irq = APIC_IRQ_LEGACY_IRQ(ih.line);
		if (irq < 0 || irq >= NUM_LEGACY_IRQS)
			irq = -1;
	}
#endif

	return intr_establish(irq, pic, pin, IST_LEVEL, level, func, arg, what);
}

void
pci_intr_disestablish(pci_chipset_tag_t pc, void *cookie)
{
	intr_disestablish(cookie);
}

struct extent *pciio_ex;
struct extent *pcimem_ex;

void
pci_init_extents(void)
{
	bios_memmap_t *bmp;
	u_int64_t size;

	if (pciio_ex == NULL) {
		/*
		 * We only have 64K of addressable I/O space.
		 * However, since BARs may contain garbage, we cover
		 * the full 32-bit address space defined by PCI of
		 * which we only make the first 64K available.
		 */
		pciio_ex = extent_create("pciio", 0, 0xffffffff, M_DEVBUF,
		    NULL, 0, EX_NOWAIT | EX_FILLED);
		if (pciio_ex == NULL)
			return;
		extent_free(pciio_ex, 0, 0x10000, M_NOWAIT);
	}

	if (pcimem_ex == NULL) {
		/*
		 * Cover the 36-bit address space addressable by PAE
		 * here.  As long as vendors continue to support
		 * 32-bit operating systems, we should never see BARs
		 * outside that region.
		 */
		pcimem_ex = extent_create("pcimem", 0, 0xfffffffff, M_DEVBUF,
		    NULL, 0, EX_NOWAIT);
		if (pcimem_ex == NULL)
			return;

		for (bmp = bios_memmap; bmp->type != BIOS_MAP_END; bmp++) {
			/*
			 * Ignore address space beyond 4G.
			 */
			if (bmp->addr >= 0x100000000ULL)
				continue;
			size = bmp->size;
			if (bmp->addr + size >= 0x100000000ULL)
				size = 0x100000000ULL - bmp->addr;

			/* Ignore zero-sized regions. */
			if (size == 0)
				continue;

			if (extent_alloc_region(pcimem_ex, bmp->addr, size,
			    EX_NOWAIT))
				printf("memory map conflict 0x%llx/0x%llx\n",
				    bmp->addr, bmp->size);
		}

		/* Take out the video buffer area and BIOS areas. */
		extent_alloc_region(pcimem_ex, IOM_BEGIN, IOM_SIZE,
		    EX_CONFLICTOK | EX_NOWAIT);
	}
}

#include "acpi.h"
#if NACPI > 0
void acpi_pci_match(struct device *, struct pci_attach_args *);
#endif

void
pci_dev_postattach(struct device *dev, struct pci_attach_args *pa)
{
#if NACPI > 0
	acpi_pci_match(dev, pa);
#endif
}
