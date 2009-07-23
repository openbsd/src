/*	$OpenBSD: pci_machdep.c,v 1.5 2009/07/23 19:24:30 miod Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/extent.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pccbbreg.h>
#include <dev/pci/ppbreg.h>
#include <dev/pci/pcidevs.h>

void	ppb_device_explore(pci_chipset_tag_t, uint, int, int, struct extent *,
	    struct extent *);
void	ppb_function_explore(pci_chipset_tag_t, pcitag_t, struct extent *,
	    struct extent *);

/*
 * Configure a PCI-PCI bridge.
 */
void
ppb_initialize(pci_chipset_tag_t pc, pcitag_t ppbtag, uint primary,
    uint secondary, uint subordinate)
{
	int dev, nfuncs;
	pcireg_t id, csr, bhlcr;
	pcitag_t tag;
	const struct pci_quirkdata *qd;
	bus_addr_t iostart, ioend, memstart, memend;
	struct extent *ioex, *memex;
	struct extent_region *region;

	/*
	 * In a first pass, enable access to the configuration space,
	 * and figure out what resources the devices behind it will
	 * need.
	 *
	 * Note that, doing this, we do not intend to support any
	 * hotplug capabilities.  This should not be a problem on
	 * sgi.
	 */

	csr = pci_conf_read(pc, ppbtag, PCI_COMMAND_STATUS_REG);
	csr &= ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE);
	pci_conf_write(pc, ppbtag, PCI_COMMAND_STATUS_REG, csr);

	pci_conf_write(pc, ppbtag, PPB_REG_BUSINFO,
	    primary | (secondary << 8) | (subordinate << 16));

	ioex = extent_create("ppb_io", 0, 0xffffffff,
	    M_DEVBUF, NULL, 0, EX_NOWAIT);
	memex = extent_create("ppb_mem", 0, 0xffffffff,
	    M_DEVBUF, NULL, 0, EX_NOWAIT);

	for (dev = 0; dev < pci_bus_maxdevs(pc, secondary); dev++) {
		tag = pci_make_tag(pc, secondary, dev, 0);
		id = pci_conf_read(pc, tag, PCI_ID_REG);

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		qd = pci_lookup_quirkdata(PCI_VENDOR(id), PCI_PRODUCT(id));
		if (PCI_HDRTYPE_MULTIFN(bhlcr) ||
		    (qd != NULL && (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0))
			nfuncs = 8;
		else
			nfuncs = 1;

		ppb_device_explore(pc, secondary, dev, nfuncs, ioex, memex);
	}

	/*
	 * Now figure out the size of the resources we need...
	 */

	iostart = memstart = 0xffffffff;
	ioend = memend = 0;

	if (ioex != NULL) {
		LIST_FOREACH(region, &ioex->ex_regions, er_link) {
			if (region->er_start < iostart)
				iostart = region->er_start;
			if (region->er_end > ioend)
				ioend = region->er_end;
		}
		extent_destroy(ioex);
	}
	if (memex != NULL) {
		LIST_FOREACH(region, &memex->ex_regions, er_link) {
			if (region->er_start < memstart)
				memstart = region->er_start;
			if (region->er_end > memend)
				memend = region->er_end;
		}
		extent_destroy(memex);
	}

	/*
	 * ... and ask the bridge to setup resources for them.
	 */

	if (pc->pc_ppb_setup == NULL || (*pc->pc_ppb_setup)(pc->pc_conf_v,
	    ppbtag, &iostart, &ioend, &memstart, &memend) != 0) {
		iostart = memstart = 0xffffffff;
		ioend = memend = 0;
	}

	pci_conf_write(pc, ppbtag, PPB_REG_MEM,
	    ((memstart & 0xfff00000) >> 16) | (memend & 0xfff00000));
	pci_conf_write(pc, ppbtag, PPB_REG_IOSTATUS,
	    (pci_conf_read(pc, ppbtag, PPB_REG_IOSTATUS) & 0xffff0000) |
	    ((iostart & 0x0000f000) >> 8) | (ioend & 0x0000f000));
	pci_conf_write(pc, ppbtag, PPB_REG_IO_HI,
	    ((iostart & 0xffff0000) >> 16) | (ioend & 0xffff0000));
	pci_conf_write(pc, ppbtag, PPB_REG_PREFMEM, 0x0000fff0);
	pci_conf_write(pc, ppbtag, PPB_REG_PREFBASE_HI32, 0);
	pci_conf_write(pc, ppbtag, PPB_REG_PREFLIM_HI32, 0);

	if (iostart <= ioend)
		csr |= PCI_COMMAND_IO_ENABLE;
	if (memstart <= memend)
		csr |= PCI_COMMAND_MEM_ENABLE;

	pci_conf_write(pc, ppbtag, PCI_COMMAND_STATUS_REG, csr |
	    PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_INVALIDATE_ENABLE |
	    PCI_COMMAND_SERR_ENABLE);
}

/*
 * Figure out what resources a device behind a bridge would need and
 * disable them.
 */
void
ppb_device_explore(pci_chipset_tag_t pc, uint bus, int dev, int nfuncs,
    struct extent *ioex, struct extent *memex)
{
	pcitag_t tag;
	pcireg_t id;
	int function;

	for (function = 0; function < nfuncs; function++) {
		tag = pci_make_tag(pc, bus, dev, function);

		id = pci_conf_read(pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		ppb_function_explore(pc, tag, ioex, memex);
	}
}

/*
 * Figure out what resources a device function would need and
 * disable them.
 */
void
ppb_function_explore(pci_chipset_tag_t pc, pcitag_t tag, struct extent *ioex,
    struct extent *memex)
{
	bus_addr_t base;
	bus_size_t size;
	int reg, reg_start, reg_end, reg_rom;
	pcireg_t csr, bhlcr, type, mask;

	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr &
	    ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE));

	bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(bhlcr)) {
	case 0:
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_END;
		reg_rom = PCI_ROM_REG;
		break;
	case 1:	/* PCI-PCI bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PPB_END;
		reg_rom = 0;	/* 0x38 */
		break;
	case 2:	/* PCI-Cardbus bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PCB_END;
		reg_rom = 0;
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
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			pci_conf_write(pc, tag, reg + 4, 0);
			/* FALLTHROUGH */
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
			if (memex != NULL)
				(void)extent_alloc(memex, size, size, 0, 0, 0,
				    &base);
			break;
		case PCI_MAPREG_TYPE_IO:
			if (ioex != NULL)
				(void)extent_alloc(ioex, size, size, 0, 0, 0,
				    &base);
			break;
		}

		pci_conf_write(pc, tag, reg, 0);

		if (type == (PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT))
			reg += 4;
	}

	if (reg_rom != 0) {
		pci_conf_write(pc, tag, reg_rom, ~PCI_ROM_ENABLE);
		mask = pci_conf_read(pc, tag, reg_rom);
		size = PCI_ROM_SIZE(mask);

		if (size != 0) {
			if (memex != NULL)
				(void)extent_alloc(memex, size, size, 0, 0, 0,
				    &base);
		}

		pci_conf_write(pc, tag, reg_rom, 0);
	}

	/*
	 * Note that we do not try to be recursive and configure PCI-PCI
	 * bridges behind PCI-PCI bridges.
	 */
}

/*
 * Configure a PCI-CardBus bridge.
 */
void
pccbb_initialize(pci_chipset_tag_t pc, pcitag_t tag, uint primary,
    uint secondary, uint subordinate)
{
	pcireg_t csr;

	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	csr &= ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);

	pci_conf_write(pc, tag, PCI_BUSNUM,
	    primary | (secondary << 8) | (subordinate << 16));

#if 0	/* done by pccbb(4) */
	csr |= PCI_COMMAND_IO_ENABLE;
	csr |= PCI_COMMAND_MEM_ENABLE;

	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr |
	    PCI_COMMAND_MASTER_ENABLE);
#endif
}
