/*      $OpenBSD: pci_map.c,v 1.11 2004/10/09 19:52:04 brad Exp $     */
/*	$NetBSD: pci_map.c,v 1.7 2000/05/10 16:58:42 thorpej Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; by William R. Studenmund; by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * PCI device mapping.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>


static int nbsd_pci_io_find(pci_chipset_tag_t, pcitag_t, int, pcireg_t,
    bus_addr_t *, bus_size_t *, int *);
static int nbsd_pci_mem_find(pci_chipset_tag_t, pcitag_t, int, pcireg_t,
    bus_addr_t *, bus_size_t *, int *);

static int
nbsd_pci_io_find(pc, tag, reg, type, basep, sizep, flagsp)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
	pcireg_t type;
	bus_addr_t *basep;
	bus_size_t *sizep;
	int *flagsp;
{
	pcireg_t address, mask;
	int s;

	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3))
		panic("pci_io_find: bad request");

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
	s = splhigh();
	address = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, 0xffffffff);
	mask = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, address);
	splx(s);

	if (PCI_MAPREG_TYPE(address) != PCI_MAPREG_TYPE_IO) {
		printf("pci_io_find: expected type i/o, found mem\n");
		return (1);
	}

	if (PCI_MAPREG_IO_SIZE(mask) == 0) {
		printf("pci_io_find: void region\n");
		return (1);
	}

	if (basep != 0)
		*basep = PCI_MAPREG_IO_ADDR(address);
	if (sizep != 0)
		*sizep = PCI_MAPREG_IO_SIZE(mask);
	if (flagsp != 0)
		*flagsp = 0;

	return (0);
}

static int
nbsd_pci_mem_find(pc, tag, reg, type, basep, sizep, flagsp)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
	pcireg_t type;
	bus_addr_t *basep;
	bus_size_t *sizep;
	int *flagsp;
{
	pcireg_t address, mask, address1 = 0, mask1 = 0xffffffff;
	u_int64_t waddress, wmask;
	int s, is64bit;

	is64bit = (PCI_MAPREG_MEM_TYPE(type) == PCI_MAPREG_MEM_TYPE_64BIT);

	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3))
		panic("pci_mem_find: bad request");

	if (is64bit && (reg + 4) >= PCI_MAPREG_END)
		panic("pci_mem_find: bad 64-bit request");

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
	s = splhigh();
	address = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, 0xffffffff);
	mask = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, address);
	if (is64bit) {
		address1 = pci_conf_read(pc, tag, reg + 4);
		pci_conf_write(pc, tag, reg + 4, 0xffffffff);
		mask1 = pci_conf_read(pc, tag, reg + 4);
		pci_conf_write(pc, tag, reg + 4, address1);
	}
	splx(s);

	if (PCI_MAPREG_TYPE(address) != PCI_MAPREG_TYPE_MEM) {
		printf("pci_mem_find: expected type mem, found i/o\n");
		return (1);
	}
	if (type != -1 && 
	    PCI_MAPREG_MEM_TYPE(address) != PCI_MAPREG_MEM_TYPE(type)) {
		printf("pci_mem_find: expected mem type %08x, found %08x\n",
		    PCI_MAPREG_MEM_TYPE(type),
		    PCI_MAPREG_MEM_TYPE(address));
		return (1);
	}

	waddress = (u_int64_t)address1 << 32UL | address;
	wmask = (u_int64_t)mask1 << 32UL | mask;

	if ((is64bit && PCI_MAPREG_MEM64_SIZE(wmask) == 0) ||
	    (!is64bit && PCI_MAPREG_MEM_SIZE(mask) == 0)) {
		printf("pci_mem_find: void region\n");
		return (1);
	}

	switch (PCI_MAPREG_MEM_TYPE(address)) {
	case PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_MEM_TYPE_32BIT_1M:
		break;
	case PCI_MAPREG_MEM_TYPE_64BIT:
		/*
		 * Handle the case of a 64-bit memory register on a
		 * platform with 32-bit addressing.  Make sure that
		 * the address assigned and the device's memory size
		 * fit in 32 bits.  We implicitly assume that if
		 * bus_addr_t is 64-bit, then so is bus_size_t.
		 */
		if (sizeof(u_int64_t) > sizeof(bus_addr_t) &&
		    (address1 != 0 || mask1 != 0xffffffff)) {
			printf("pci_mem_find: 64-bit memory map which is "
			    "inaccessible on a 32-bit platform\n");
			return (1);
		}
		break;
	default:
		printf("pci_mem_find: reserved mapping register type\n");
		return (1);
	}

	if (sizeof(u_int64_t) > sizeof(bus_addr_t)) {
		if (basep != 0)
			*basep = PCI_MAPREG_MEM_ADDR(address);
		if (sizep != 0)
			*sizep = PCI_MAPREG_MEM_SIZE(mask);
	} else {
		if (basep != 0)
			*basep = PCI_MAPREG_MEM64_ADDR(waddress);
		if (sizep != 0)
			*sizep = PCI_MAPREG_MEM64_SIZE(wmask);
	}
	if (flagsp != 0)
		*flagsp =
#ifdef BUS_SPACE_MAP_PREFETCHABLE
		    PCI_MAPREG_MEM_PREFETCHABLE(address) ?
		      BUS_SPACE_MAP_PREFETCHABLE :
#endif
		  0;

	return (0);
}

int
pci_io_find(pc, pcitag, reg, iobasep, iosizep)
	pci_chipset_tag_t pc;
	pcitag_t pcitag;
	int reg;
	bus_addr_t *iobasep;
	bus_size_t *iosizep;
{
	return (nbsd_pci_io_find(pc, pcitag, reg, 0, iobasep, iosizep, 0));
}

int
pci_mem_find(pc, pcitag, reg, membasep, memsizep, cacheablep)
	pci_chipset_tag_t pc;
	pcitag_t pcitag;
	int reg;
	bus_addr_t *membasep;
	bus_size_t *memsizep;
	int *cacheablep;
{
	return (nbsd_pci_mem_find(pc, pcitag, reg, -1, membasep, memsizep,
				  cacheablep));
}

pcireg_t
pci_mapreg_type(pc, tag, reg)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
{
	pcireg_t rv;

	rv = pci_conf_read(pc, tag, reg);
	if (PCI_MAPREG_TYPE(rv) == PCI_MAPREG_TYPE_IO)
		rv &= PCI_MAPREG_TYPE_MASK;
	else
		rv &= PCI_MAPREG_TYPE_MASK|PCI_MAPREG_MEM_TYPE_MASK;
	return (rv);
}

int
pci_mapreg_info(pc, tag, reg, type, basep, sizep, flagsp)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
	pcireg_t type;
	bus_addr_t *basep;
	bus_size_t *sizep;
	int *flagsp;
{

	if (PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_IO)
		return (nbsd_pci_io_find(pc, tag, reg, type, basep, sizep,
		    flagsp));
	else
		return (nbsd_pci_mem_find(pc, tag, reg, type, basep, sizep,
		    flagsp));
}

int
pci_mapreg_map(pa, reg, type, busflags, tagp, handlep, basep, sizep, maxsize)
	struct pci_attach_args *pa;
	int reg, busflags;
	pcireg_t type;
	bus_space_tag_t *tagp;
	bus_space_handle_t *handlep;
	bus_addr_t *basep;
	bus_size_t *sizep;
	bus_size_t maxsize;
{
	bus_space_tag_t tag;
	bus_space_handle_t handle;
	bus_addr_t base;
	bus_size_t size;
	int flags;

	if (PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_IO) {
		if ((pa->pa_flags & PCI_FLAGS_IO_ENABLED) == 0)
			return (1);
		if (nbsd_pci_io_find(pa->pa_pc, pa->pa_tag, reg, type, &base,
				     &size, &flags))
			return (1);
		tag = pa->pa_iot;
	} else {
		if ((pa->pa_flags & PCI_FLAGS_MEM_ENABLED) == 0)
			return (1);
		if (nbsd_pci_mem_find(pa->pa_pc, pa->pa_tag, reg, type, &base,
				      &size, &flags))
			return (1);
		tag = pa->pa_memt;
	}

	/* The caller can request limitation of the mapping's size. */
	if (maxsize != 0 && size > maxsize) {
#ifdef DEBUG
		printf("pci_mapreg_map: limited PCI mapping from %lx to %lx\n",
		    (u_long)size, (u_long)maxsize);
#endif
		size = maxsize;
	}

	if (bus_space_map(tag, base, size, busflags | flags, &handle))
		return (1);

	if (tagp != NULL)
		*tagp = tag;
	if (handlep != NULL)
		*handlep = handle;
	if (basep != NULL)
		*basep = base;
	if (sizep != NULL)
		*sizep = size;

	return (0);
}
