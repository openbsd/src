/*	$NetBSD: apecs_pci.c,v 1.3 1995/08/03 01:16:57 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/pio.h>

#include <dev/isa/isavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/pci_chipset.h>
#include <alpha/pci/apecsreg.h>

void	 apecs_setup __P((void));
pcitag_t apecs_make_tag __P((int, int, int));
pcireg_t apecs_conf_read __P((pcitag_t, int));
void	 apecs_conf_write __P((pcitag_t, int, pcireg_t));
int	 apecs_map_io __P((pcitag_t, int, int *));
int	 apecs_map_mem __P((pcitag_t, int, vm_offset_t *, vm_offset_t *));
int	 apecs_pcidma_map __P((caddr_t, vm_size_t, vm_offset_t *));
void	 apecs_pcidma_unmap __P((caddr_t, vm_size_t, int, vm_offset_t *));

struct pci_cs_fcns apecs_p1e_cs_fcns = {	/* XXX WHAT'S DIFFERENT? */
	apecs_setup,
	apecs_make_tag,
	apecs_conf_read,
	apecs_conf_write,
	apecs_map_io,
	apecs_map_mem,
	apecs_pcidma_map,
	apecs_pcidma_unmap,
};

struct pci_cs_fcns apecs_p2e_cs_fcns = {	/* XXX WHAT'S DIFFERENT? */
	apecs_setup,
	apecs_make_tag,
	apecs_conf_read,
	apecs_conf_write,
	apecs_map_io,
	apecs_map_mem,
	apecs_pcidma_map,
	apecs_pcidma_unmap,
};

#define	REGVAL(r)	(*(u_int32_t *)phystok0seg(r))

void
apecs_setup()
{

	/*
	 * Set up PCI bus mastering DMA windows on the APECS chip.
	 *
	 * What the PROM wants:
	 *	a 1G direct-mapped window that maps the PCI address
	 *	space from 4G -> 5G to memory addresses 0 -> 1G,
	 *	set up in window two.
	 *
	 * What we want:
	 *	a 1G direct-mapped window that maps the PCI address
	 *	space from 0 -> 1G to memory addresses 0 -> 1G.
	 *
	 * Unless we satisfy the PROM, we can't live through a reboot.
	 * If we don't do what we want, I have to write more code.

	 * So:
	 *	Leave window two alone, map window 1 the way I want it.
	 *
	 * XXX verify that windows don't overlap
	 * XXX be trickier
	 * XXX magic numbers
	 */

#if 0 /* should be routine to dump regs */
	printf("old base1  was 0x%x\n", REGVAL(EPIC_PCI_BASE_1));
	printf("old mask1  was 0x%x\n", REGVAL(EPIC_PCI_MASK_1));
	printf("old tbase1 was 0x%x\n", REGVAL(EPIC_TBASE_1));

	printf("old base2  was 0x%x\n", REGVAL(EPIC_PCI_BASE_2));
	printf("old mask2  was 0x%x\n", REGVAL(EPIC_PCI_MASK_2));
	printf("old tbase2 was 0x%x\n", REGVAL(EPIC_TBASE_2));
#endif

#if 0 /* XXX STUPID PROM; MUST LEAVE WINDOW 2 ALONE.  See above */
        /* Turn off DMA window enables in PCI Base Reg 2. */
        REGVAL(EPIC_PCI_BASE_2) = 0;

        /* Set up Translated Base Register 2; translate to sybBus addr 0. */
	REGVAL(EPIC_TBASE_2) = 0;

	/* Set up PCI mask register 2; map 1G space. */
	REGVAL(EPIC_PCI_MASK_2) = 0x3ff00000;

	/* Enable window 2; from PCI address 4G, direct mapped. */
	REGVAL(EPIC_PCI_BASE_2) = 0x40080000;
#endif /* STUPID PROM */

        /* Turn off DMA window enables in PCI Base Reg 1. */
        REGVAL(EPIC_PCI_BASE_1) = 0;

        /* Set up Translated Base Register 1; translate to sybBus addr 0. */
{ /* XXX */
extern struct sgmapent *sgmap;
	REGVAL(EPIC_TBASE_1) = vtophys(sgmap) >> 1;
} /* XXX */

	/* Set up PCI mask register 1; map 8MB space. */
	REGVAL(EPIC_PCI_MASK_1) = 0x00700000;

	/* Enable window 1; from PCI address 8MB, direct mapped. */
	REGVAL(EPIC_PCI_BASE_1) = 0x008c0000;

	/*
	 * Should set up HAXR1 and HAXR2...  However, the PROM again
	 * wants them where they're set to be...
	 */
#if 0
	printf("old haxr0  was 0x%x\n", REGVAL(EPIC_HAXR0));
	printf("old haxr1  was 0x%x\n", REGVAL(EPIC_HAXR1));
	printf("old haxr2  was 0x%x\n", REGVAL(EPIC_HAXR2));
#endif

#if 0 /* XXX STUPID PROM */
	/* HAXR0 is wired zero; no op. */
	REGVAL(EPIC_HAXR0) = 0;

	/* HAXR1: maps PCI memory space above 16M.  16M -> 2G+16M. */
	REGVAL(EPIC_HAXR1) = 0x80000000;

	/* HAXR2: maps PCI I/O space above 256K.  256K -> 256k. */
	REGVAL(EPIC_HAXR2) = 0;
#endif
}

pcitag_t
apecs_make_tag(bus, device, function)
	int bus, device, function;
{
	pcitag_t tag;

	if (bus >= 256 || device >= 32 || function >= 8)
		panic("apecs_make_tag: bad request");

	tag = (bus << 21) | (device << 16) | (function << 13);
#if 0
	printf("apecs_make_tag: bus %d, device %d, function %d -> 0x%lx\n", bus,
	    device, function, tag);
#endif
	return tag;
}

pcireg_t
apecs_conf_read(tag, offset)
	pcitag_t tag;
	int offset;					/* XXX */
{
	pcireg_t *datap, data;
	int reg = offset >> 2;				/* XXX */

	if ((tag & 0x1fe00000) != 0) {
		panic("apecs_conf_read: bus != 0?");
	}
	/* XXX FILL IN HAXR2 bits. */

	datap = (pcireg_t *)
	    phystok0seg(APECS_PCI_CONF | tag | reg << 7 | 0 << 5 | 0x3 << 3);
	if (badaddr(datap, sizeof *datap))
		return ((pcireg_t)-1);
	data = *datap;
#if 0
	printf("apecs_conf_read: tag 0x%lx, reg 0x%lx -> %x @ %p\n", tag, reg,
	    data, datap);
#endif
	return data;
}

void
apecs_conf_write(tag, offset, data)
	pcitag_t tag;
	int offset;					/* XXX */
	pcireg_t data;
{
	pcireg_t *datap;
	int reg = offset >> 2;				/* XXX */

	if ((tag & 0x1fe00000) != 0) {
		panic("apecs_conf_read: bus != 0?");
	}
	/* XXX FILL IN HAXR2 bits. */

	datap = (pcireg_t *)
	    phystok0seg(APECS_PCI_CONF | tag | reg << 7 | 0 << 5 | 0x3 << 3);
#if 0
	printf("apecs_conf_write: tag 0x%lx, reg 0x%lx -> 0x%x @ %p\n", tag,
	    reg, data, datap);
#endif
	*datap = data;
}

int
apecs_map_io(tag, reg, iobasep)
	pcitag_t tag;
	int reg;
	int *iobasep;
{
	pcireg_t data;
	int pci_iobase;

	if (reg < PCI_MAP_REG_START || reg >= PCI_MAP_REG_END || (reg & 3))
		panic("apecs_map_io: bad request");

	data = pci_conf_read(tag, reg);

	if ((data & PCI_MAP_IO) == 0)
		panic("apecs_map_io: attempt to I/O map an memory region");

	/* figure out where it was mapped... */
	pci_iobase = data & PCI_MAP_MEMORY_ADDRESS_MASK; /* PCI I/O addr */

	return (pci_iobase);
}

int
apecs_map_mem(tag, reg, vap, pap)
	pcitag_t tag;
	int reg;
	vm_offset_t *vap, *pap;
{
	pcireg_t data;
	vm_offset_t pci_pa, sb_pa;

	if (reg < PCI_MAP_REG_START || reg >= PCI_MAP_REG_END || (reg & 3))
		panic("apecs_map_mem: bad request");

	/*
	 * "HERE WE GO AGAIN!!!"
	 *
	 * The PROM has already mapped the device for us.  The PROM is
	 * our friend.  We wouldn't want to make the PROM unhappy.
	 *
	 * So, we take the address that's been assigned (already) to
	 * the register, and figure out what physical and virtual addresses
	 * go with it...
	 */
	/*
	 * Section 6.2.5.1, `Address Maps', says that a device which wants 2^n
	 * bytes of memory will hardwire the bottom n bits of the address to 0.
	 * As recommended, we write all 1s and see what we get back.
	 */
	data = pci_conf_read(tag, reg);

	if (data & PCI_MAP_IO)
		panic("apecs_map_mem: attempt to memory map an I/O region");

	switch (data & PCI_MAP_MEMORY_TYPE_MASK) {
	case PCI_MAP_MEMORY_TYPE_32BIT:
		break;
	case PCI_MAP_MEMORY_TYPE_32BIT_1M:
		printf("apecs_map_mem: attempt to map restricted 32-bit region\n");
		return EOPNOTSUPP;
	case PCI_MAP_MEMORY_TYPE_64BIT:
		printf("apecs_map_mem: attempt to map 64-bit region\n");
		return EOPNOTSUPP;
	default:
		printf("apecs_map_mem: reserved mapping type\n");
		return EINVAL;
	}

	/* figure out where it was mapped... */
	pci_pa = data & PCI_MAP_MEMORY_ADDRESS_MASK;	/* PCI bus address */

	/* calcluate sysBus address -- should be a better way to get space */
	if (data & PCI_MAP_MEMORY_CACHABLE) {
		/* Dense space */
		sb_pa = (pci_pa & 0xffffffff) | (3L << 32);	/* XXX */
	} else {
		/* Sparse space */
		sb_pa = ((pci_pa & 0x7ffffff) << 5) | (2L << 32); /* XXX */
	}

	/* and tell the driver. */
	*vap = phystok0seg(sb_pa);
	*pap = pci_pa;

#if 0
	printf("pci_map_mem: memory mapped at 0x%lx\n", *pap);
	printf("pci_map_mem: virtual 0x%lx\n", *vap);
#endif

	return 0;
}

int
apecs_pcidma_map(addr, size, mappings)
	caddr_t addr;
	vm_size_t size;
	vm_offset_t *mappings;
{
	vm_offset_t va;
	long todo;
	int i;

	i = 0;
	va = (vm_offset_t)addr;
	todo = size;

	while (todo > 0) {
		mappings[i] = vtophys(va) | 0x40000000;
#if 0
		printf("a_pd_m mapping %d: %lx -> %lx -> %lx\n", i, va,
		    vtophys(va), mappings[i]);
#endif
		i++;
		todo -= PAGE_SIZE - (va - trunc_page(va));
		va += PAGE_SIZE - (va - trunc_page(va));
	}
	return (i);
}

void
apecs_pcidma_unmap(addr, size, nmappings, mappings)
	caddr_t addr;
	vm_size_t size;
	int nmappings;
	vm_offset_t *mappings;
{

	/* maybe XXX if diagnostic, check that mapping happened. */
	printf("apecs_pcidma_unmap: nada\n");
}
