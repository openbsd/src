/*	$NetBSD: pci_machdep.c,v 1.18 1995/12/24 02:30:34 mycroft Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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
 *
 * On PCs, there are two methods of generating PCI configuration cycles.
 * We try to detect the appropriate mechanism for this machine and set
 * up a few function pointers to access the correct method directly.
 *
 * The configuration method can be hard-coded in the config file by
 * using `options PCI_CONF_MODE=N', where `N' is the configuration mode
 * as defined section 3.6.4.1, `Generating Configuration Cycles'.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/pio.h>

#include <i386/isa/icu.h>
#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

int pci_mode = -1;

int pcimatch __P((struct device *, void *, void *));
void pciattach __P((struct device *, struct device *, void *));

struct cfdriver pcicd = {
	NULL, "pci", pcimatch, pciattach, DV_DULL, sizeof(struct device)
};

int
pcimatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{

	if (pci_mode_detect() == 0)
		return 0;
	return 1;
}

void
pciattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	int bus, device;

	printf(": configuration mode %d\n", pci_mode);

#if 0
	for (bus = 0; bus <= 255; bus++)
#else
	/*
	 * XXX
	 * Some current chipsets do wacky things with bus numbers > 0.
	 * This seems like a violation of protocol, but the PCI BIOS does
	 * allow one to query the maximum bus number, and eventually we
	 * should do so.
	 */
	for (bus = 0; bus <= 0; bus++)
#endif
		for (device = 0; device <= (pci_mode == 2 ? 15 : 31); device++)
			pci_attach_subdev(self, bus, device);
}

#define	PCI_MODE1_ENABLE	0x80000000UL
#define	PCI_MODE1_ADDRESS_REG	0x0cf8
#define	PCI_MODE1_DATA_REG	0x0cfc

#define	PCI_MODE2_ENABLE_REG	0x0cf8
#define	PCI_MODE2_FORWARD_REG	0x0cfa

pcitag_t
pci_make_tag(bus, device, function)
	int bus, device, function;
{
	pcitag_t tag;

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_make_tag: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
mode1:
	if (bus >= 256 || device >= 32 || function >= 8)
		panic("pci_make_tag: bad request");

	tag.mode1 = PCI_MODE1_ENABLE |
		    (bus << 16) | (device << 11) | (function << 8);
	return tag;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
mode2:
	if (bus >= 256 || device >= 16 || function >= 8)
		panic("pci_make_tag: bad request");

	tag.mode2.port = 0xc000 | (device << 8);
	tag.mode2.enable = 0xf0 | (function << 1);
	tag.mode2.forward = bus;
	return tag;
#endif
}

pcireg_t
pci_conf_read(tag, reg)
	pcitag_t tag;
	int reg;
{
	pcireg_t data;

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_conf_read: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
mode1:
	outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
	data = inl(PCI_MODE1_DATA_REG);
	outl(PCI_MODE1_ADDRESS_REG, 0);
	return data;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
mode2:
	outb(PCI_MODE2_ENABLE_REG, tag.mode2.enable);
	outb(PCI_MODE2_FORWARD_REG, tag.mode2.forward);
	data = inl(tag.mode2.port | reg);
	outb(PCI_MODE2_ENABLE_REG, 0);
	return data;
#endif
}

void
pci_conf_write(tag, reg, data)
	pcitag_t tag;
	int reg;
	pcireg_t data;
{

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_conf_write: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
mode1:
	outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
	outl(PCI_MODE1_DATA_REG, data);
	outl(PCI_MODE1_ADDRESS_REG, 0);
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
mode2:
	outb(PCI_MODE2_ENABLE_REG, tag.mode2.enable);
	outb(PCI_MODE2_FORWARD_REG, tag.mode2.forward);
	outl(tag.mode2.port | reg, data);
	outb(PCI_MODE2_ENABLE_REG, 0);
#endif
}

int
pci_mode_detect()
{

#ifdef PCI_CONF_MODE
#if (PCI_CONF_MODE == 1) || (PCI_CONF_MODE == 2)
	return (pci_mode = PCI_CONF_MODE);
#else
#error Invalid PCI configuration mode.
#endif
#else
	if (pci_mode != -1)
		return pci_mode;

	/*
	 * We try to divine which configuration mode the host bridge wants.  We
	 * try mode 2 first, because our probe for mode 1 is likely to succeed
	 * for mode 2 also.
	 *
	 * XXX
	 * This should really be done using the PCI BIOS.
	 */

	outb(PCI_MODE2_ENABLE_REG, 0);
	outb(PCI_MODE2_FORWARD_REG, 0);
	if (inb(PCI_MODE2_ENABLE_REG) != 0 ||
	    inb(PCI_MODE2_FORWARD_REG) != 0)
		goto not2;
	return (pci_mode = 2);

not2:
	outl(PCI_MODE1_ADDRESS_REG, PCI_MODE1_ENABLE);
	if (inl(PCI_MODE1_ADDRESS_REG) != PCI_MODE1_ENABLE)
		goto not1;
	outl(PCI_MODE1_ADDRESS_REG, 0);
	if (inl(PCI_MODE1_ADDRESS_REG) != 0)
		goto not1;
	return (pci_mode = 1);

not1:
	return (pci_mode = 0);
#endif
}

int
pci_map_io(tag, reg, iobasep)
	pcitag_t tag;
	int reg;
	int *iobasep;
{
	pcireg_t address;
	int iobase;

	if (reg < PCI_MAP_REG_START || reg >= PCI_MAP_REG_END || (reg & 3))
		panic("pci_map_io: bad request");

	address = pci_conf_read(tag, reg);

	if ((address & PCI_MAP_IO) == 0)
		panic("pci_map_io: attempt to I/O map a memory region");

	iobase = address & PCI_MAP_IO_ADDRESS_MASK;
	*iobasep = iobase;

	return 0;
}

int
pci_map_mem(tag, reg, vap, pap)
	pcitag_t tag;
	int reg;
	vm_offset_t *vap, *pap;
{
	pcireg_t address, mask;
	int cachable;
	vm_size_t size;
	vm_offset_t va, pa;

	if (reg < PCI_MAP_REG_START || reg >= PCI_MAP_REG_END || (reg & 3))
		panic("pci_map_mem: bad request");

	/*
	 * Section 6.2.5.1, `Address Maps', tells us that:
	 *
	 * 1) The builtin software should have already mapped the device in a
	 * reasonable way.
	 *
	 * 2) A device which wants 2^n bytes of memory will hardwire the bottom
	 * n bits of the address to 0.  As recommended, we write all 1s and see
	 * what we get back.
	 */
	address = pci_conf_read(tag, reg);
	pci_conf_write(tag, reg, 0xffffffff);
	mask = pci_conf_read(tag, reg);
	pci_conf_write(tag, reg, address);

	if ((address & PCI_MAP_IO) != 0)
		panic("pci_map_mem: attempt to memory map an I/O region");

	switch (address & PCI_MAP_MEMORY_TYPE_MASK) {
	case PCI_MAP_MEMORY_TYPE_32BIT:
	case PCI_MAP_MEMORY_TYPE_32BIT_1M:
		break;
	case PCI_MAP_MEMORY_TYPE_64BIT:
		printf("pci_map_mem: attempt to map 64-bit region\n");
		return EOPNOTSUPP;
	default:
		printf("pci_map_mem: reserved mapping type\n");
		return EINVAL;
	}

	pa = address & PCI_MAP_MEMORY_ADDRESS_MASK;
	size = -(mask & PCI_MAP_MEMORY_ADDRESS_MASK);
	if (size < NBPG)
		size = NBPG;

	va = kmem_alloc_pageable(kernel_map, size);
	if (va == 0) {
		printf("pci_map_mem: not enough memory\n");
		return ENOMEM;
	}

	/*
	 * Tell the driver where we mapped it.
	 *
	 * If the region is smaller than one page, adjust the virtual address
	 * to the same page offset as the physical address.
	 */
	*vap = va + (pa & PGOFSET);
	*pap = pa;

#if 1
	printf("pci_map_mem: mapping memory at virtual %08x, physical %08x\n", *vap, *pap);
#endif

	/* Map the space into the kernel page table. */
	cachable = !!(address & PCI_MAP_MEMORY_CACHABLE);
	pa &= ~PGOFSET;
	while (size) {
		pmap_enter(pmap_kernel(), va, pa, VM_PROT_READ | VM_PROT_WRITE,
		    TRUE);
		if (!cachable)
			pmap_changebit(pa, PG_N, ~0);
		else
			pmap_changebit(pa, 0, ~PG_N);
		va += NBPG;
		pa += NBPG;
		size -= NBPG;
	}

	return 0;
}

void *
pci_map_int(tag, level, func, arg)
	pcitag_t tag;
	int level;
	int (*func) __P((void *));
	void *arg;
{
	pcireg_t data;
	int pin, line;

	data = pci_conf_read(tag, PCI_INTERRUPT_REG);

	pin = PCI_INTERRUPT_PIN(data);
	line = PCI_INTERRUPT_LINE(data);

	if (pin == 0) {
		/* No IRQ used. */
		return 0;
	}

	if (pin > 4) {
		printf("pci_map_int: bad interrupt pin %d\n", pin);
		return NULL;
	}

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
	if (line == 0 || line == 255) {
		printf("pci_map_int: no mapping for pin %c\n", '@' + pin);
		return NULL;
	} else {
		if (line >= ICU_LEN) {
			printf("pci_map_int: bad interrupt line %d\n", line);
			return NULL;
		}
		if (line == 2) {
			printf("pci_map_int: changed line 2 to line 9\n");
			line = 9;
		}
	}

#if 1
	printf("pci_map_int: pin %c mapped to line %d\n", '@' + pin, line);
#endif

	return isa_intr_establish(line, IST_LEVEL, level, func, arg);
}
