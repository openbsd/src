/*	$OpenBSD: macepcimap.c,v 1.3 2004/09/09 22:11:39 pefo Exp $ */
/*	$NetBSD: pci_mace.c,v 1.2 2004/01/19 10:28:28 sekiya Exp $	*/

/*
 * Copyright (c) 2001,2003 Christopher Sekiya
 * Copyright (c) 2000 Soren S. Jorvang
 * All rights reserved.
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <machine/vmparam.h>
#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>


void pciaddr_remap(pci_chipset_tag_t);
void pciaddr_resource_manage(pci_chipset_tag_t, pcitag_t, void *);
bus_addr_t pciaddr_ioaddr(u_int32_t);
int pciaddr_do_resource_allocate(pci_chipset_tag_t, pcitag_t, int, void *,
	int, bus_addr_t *, bus_size_t);
void pciaddr_print_devid(pci_chipset_tag_t, pcitag_t);


#define PAGE_ALIGN(x)	(((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define MEG_ALIGN(x)	(((x) + 0x100000 - 1) & ~(0x100000 - 1))


unsigned int ioaddr_base = 0x1000;
unsigned int memaddr_base = 0x80100000;

#ifdef DEBUG
int pcibiosverbose = 1;
#endif


void
pciaddr_remap(pci_chipset_tag_t pc)
{
	pcitag_t devtag;
	int device;

	/* Must fix up all PCI devices, ahc_pci expects proper i/o mapping */
	for (device = 1; device < 4; device++) {
		const struct pci_quirkdata *qd;
		int function, nfuncs;
		pcireg_t bhlcr, id;

		devtag = pci_make_tag(pc, 0, device, 0);
		id = pci_conf_read(pc, devtag, PCI_ID_REG);

		/* Invalid vendor ID value? */
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
			continue;
		/* XXX Not invalid, but we've done this ~forever. */
		if (PCI_VENDOR(id) == 0)
			continue;

		qd = pci_lookup_quirkdata(PCI_VENDOR(id), PCI_PRODUCT(id));
		bhlcr = pci_conf_read(pc, devtag, PCI_BHLC_REG);

		if (PCI_HDRTYPE_MULTIFN(bhlcr) ||
		    (qd != NULL &&
		    (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0))
			nfuncs = 8;
		else
			nfuncs = 1;

		for (function = 0; function < nfuncs; function++) {
			devtag = pci_make_tag(pc, 0, device, function);
			id = pci_conf_read(pc, devtag, PCI_ID_REG);

			/* Invalid vendor ID value? */
			if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
				continue;
			/* Not invalid, but we've done this ~forever */
			if (PCI_VENDOR(id) == 0)
				continue;

			pciaddr_resource_manage(pc, devtag, NULL);
		}
	}
}


void
pciaddr_resource_manage(pc, tag, ctx)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	void *ctx;
{
	pcireg_t val, mask;
	bus_addr_t addr;
	bus_size_t size;
	int error, mapreg, type, reg_start, reg_end, width;

	val = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(val)) {
	default:
		printf("WARNING: unknown PCI device header.");
		return;
	case 0:
		reg_start = PCI_MAPREG_START;
		reg_end   = PCI_MAPREG_END;
		break;
	case 1: /* PCI-PCI bridge */
		reg_start = PCI_MAPREG_START;
		reg_end   = PCI_MAPREG_PPB_END;
		break;
	case 2: /* PCI-CardBus bridge */
		reg_start = PCI_MAPREG_START;
		reg_end   = PCI_MAPREG_PCB_END;
		break;
	}
	error = 0;

	for (mapreg = reg_start; mapreg < reg_end; mapreg += width) {
		/* inquire PCI device bus space requirement */
		val = pci_conf_read(pc, tag, mapreg);
		pci_conf_write(pc, tag, mapreg, ~0);

		mask = pci_conf_read(pc, tag, mapreg);
		pci_conf_write(pc, tag, mapreg, val);

		type = PCI_MAPREG_TYPE(val);
		width = 4;

		if (type == PCI_MAPREG_TYPE_MEM) {
			size = PCI_MAPREG_MEM_SIZE(mask);

			/*
			 * XXXrkb: for MEM64 BARs, to be totally kosher
			 * about the requested size, need to read mask
			 * from top 32bits of BAR and stir that into the
			 * size calculation, like so:
			 *
			 * case PCI_MAPREG_MEM_TYPE_64BIT:
			 *	bar64 = pci_conf_read(pb->pc, tag, br + 4);
			 *	pci_conf_write(pb->pc, tag, br + 4, 0xffffffff);
			 *	mask64 = pci_conf_read(pb->pc, tag, br + 4);
			 *	pci_conf_write(pb->pc, tag, br + 4, bar64);
			 *	size = (u_int64_t) PCI_MAPREG_MEM64_SIZE(
			 *	      (((u_int64_t) mask64) << 32) | mask);
			 *	width = 8;
			 *
			 * Fortunately, anything with all-zeros mask in the
			 * lower 32-bits will have size no less than 1 << 32,
			 * which we're not prepared to deal with, so I don't
			 * feel bad punting on it...
			 */
			if (PCI_MAPREG_MEM_TYPE(val) ==
			    PCI_MAPREG_MEM_TYPE_64BIT) {
				/*
				 * XXX We could examine the upper 32 bits
				 * XXX of the BAR here, but we are totally
				 * XXX unprepared to handle a non-zero value,
				 * XXX either here or anywhere else in the
				 * XXX sgimips code (not sure about MI code).
				 * XXX
				 * XXX So just arrange to skip the top 32
				 * XXX bits of the BAR and zero then out
				 * XXX if the BAR is in use.
				 */
				width = 8;

				if (size != 0)
					pci_conf_write(pc, tag,
					    mapreg + 4, 0);
			}
		} else {
			/*
			 * Upper 16 bits must be one.  Devices may hardwire
			 * them to zero, though, per PCI 2.2, 6.2.5.1, p 203.
			 */
			mask |= 0xffff0000;
			size = PCI_MAPREG_IO_SIZE(mask);
		}

		if (size == 0) /* unused register */
			continue;

		addr = pciaddr_ioaddr(val);

		/* reservation/allocation phase */
		error += pciaddr_do_resource_allocate(pc, tag, mapreg,
		    ctx, type, &addr, size);

	}

	/* enable/disable PCI device */
	val = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);

	if (error == 0)
		val |= (PCI_COMMAND_IO_ENABLE |
			PCI_COMMAND_MEM_ENABLE |
			PCI_COMMAND_MASTER_ENABLE |
			PCI_COMMAND_SPECIAL_ENABLE |
			PCI_COMMAND_INVALIDATE_ENABLE |
			PCI_COMMAND_PARITY_ENABLE);
	else
		val &= ~(PCI_COMMAND_IO_ENABLE |
			 PCI_COMMAND_MEM_ENABLE |
			 PCI_COMMAND_MASTER_ENABLE);

	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, val);

}

bus_addr_t
pciaddr_ioaddr(val)
	u_int32_t val;
{

	return ((PCI_MAPREG_TYPE(val) == PCI_MAPREG_TYPE_MEM) ?
	    PCI_MAPREG_MEM_ADDR(val) : PCI_MAPREG_IO_ADDR(val));
}

int
pciaddr_do_resource_allocate(pc, tag, mapreg, ctx, type, addr, size)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	void *ctx;
	int mapreg, type;
	bus_addr_t *addr;
	bus_size_t size;
{

	switch (type) {
	case PCI_MAPREG_TYPE_IO:
		*addr = ioaddr_base;
		ioaddr_base += PAGE_ALIGN(size);
		break;

	case PCI_MAPREG_TYPE_MEM:
		*addr = memaddr_base;
		memaddr_base += MEG_ALIGN(size);
		break;

	default:
		printf("attempt to remap unknown region (addr 0x%lx, "
		    "size 0x%lx, type %d)\n", *addr, size, type);
		return 0;
	}


	/* write new address to PCI device configuration header */
	pci_conf_write(pc, tag, mapreg, *addr);

	/* check */
#ifdef DEBUG
	if (!pcibiosverbose) {
		printf("pci_addr_fixup: ");
		pciaddr_print_devid(pc, tag);
	}
#endif
	if (pciaddr_ioaddr(pci_conf_read(pc, tag, mapreg)) != *addr) {
		pci_conf_write(pc, tag, mapreg, 0); /* clear */
		printf("fixup failed. (new address=%#x)\n", (unsigned)*addr);
		return (1);
	}
#ifdef DEBUG
	if (!pcibiosverbose)
		printf("new address 0x%08x (size 0x%x)\n", (unsigned)*addr,
		    (unsigned)size);
#endif

	return (0);
}

void
pciaddr_print_devid(pci_chipset_tag_t pc, pcitag_t tag)
{
	int bus, device, function;
	pcireg_t id;

	id = pci_conf_read(pc, tag, PCI_ID_REG);
	pci_decompose_tag(pc, tag, &bus, &device, &function);
	printf("%03d:%02d:%d 0x%04x 0x%04x ", bus, device, function,
	    PCI_VENDOR(id), PCI_PRODUCT(id));
}

