/*	$OpenBSD: pci_addr_fixup.c,v 1.2 2002/09/15 02:02:44 deraadt Exp $	*/
/*	$NetBSD: pci_addr_fixup.c,v 1.7 2000/08/03 20:10:45 nathanw Exp $	*/

/*-
 * Copyright (c) 2000 UCHIYAMA Yasushi.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/extent.h>

#include <uvm/uvm_param.h>
#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <macppc/pci/pcibrvar.h>

typedef int (*pciaddr_resource_manage_func_t)(struct pcibr_softc *,
	pci_chipset_tag_t, pcitag_t, int, struct extent *, int, bus_addr_t *,
	bus_size_t);
void	pciaddr_resource_manage(struct pcibr_softc *,
    pci_chipset_tag_t, pcitag_t, pciaddr_resource_manage_func_t);
void	pciaddr_resource_reserve(struct pcibr_softc *,
    pci_chipset_tag_t, pcitag_t);
int	pciaddr_do_resource_reserve(struct pcibr_softc *,
    pci_chipset_tag_t, pcitag_t, int, struct extent *, int,
    bus_addr_t *, bus_size_t);
void	pciaddr_resource_allocate(struct pcibr_softc *,
    pci_chipset_tag_t, pcitag_t);
int	pciaddr_do_resource_allocate(struct pcibr_softc *,
    pci_chipset_tag_t, pcitag_t, int, struct extent *, int, bus_addr_t *,
    bus_size_t);
bus_addr_t pciaddr_ioaddr(u_int32_t);
void	pciaddr_print_devid(pci_chipset_tag_t, pcitag_t);

int	pciaddr_device_is_agp(pci_chipset_tag_t, pcitag_t);

void pci_device_foreach(struct pcibr_softc *sc, pci_chipset_tag_t pc,
	int maxbus,
	void (*func)(struct pcibr_softc *, pci_chipset_tag_t, pcitag_t));

#define PCIADDR_MEM_START	0x0
#define PCIADDR_MEM_END		0xffffffff
#define PCIADDR_PORT_START	0x0
#define PCIADDR_PORT_END	0xffff

int pcibr_flags = 0;
#define PCIBR_VERBOSE		1
#define PCIBR_ADDR_FIXUP	2

#define PCIBIOS_PRINTV(x)  if (pcibr_flags & PCIBR_VERBOSE) \
	printf x

void
pci_addr_fixup(sc, pc, maxbus)
	struct pcibr_softc *sc;
	pci_chipset_tag_t pc;
	int maxbus;
{
	const char *verbose_header =
		"[%s]-----------------------\n"
		"  device vendor product\n"
		"  register space address    size\n"
		"--------------------------------------------\n";
	const char *verbose_footer =
		"--------------------------[%3d devices bogus]\n";

	sc->extent_mem = extent_create("PCI I/O memory space",
	    sc->sc_membus_space.bus_base,
	    sc->sc_membus_space.bus_base + sc->sc_membus_space.bus_size,
	    M_DEVBUF, 0, 0, EX_NOWAIT);
	KASSERT(sc->extent_mem);
	sc->extent_port = extent_create("PCI I/O port space",
#if 1
	    sc->sc_iobus_space.bus_base,
	    sc->sc_iobus_space.bus_base + sc->sc_iobus_space.bus_size,
#else
	    PCIADDR_PORT_START, PCIADDR_PORT_END,
#endif
	    M_DEVBUF, 0, 0, EX_NOWAIT);
	KASSERT(sc->extent_port);

	/*
	 * 1. check & reserve system BIOS setting.
	 */
	PCIBIOS_PRINTV((verbose_header, "System BIOS Setting"));
	pci_device_foreach(sc, pc, maxbus, pciaddr_resource_reserve);
	PCIBIOS_PRINTV((verbose_footer, sc->nbogus));

	{
		struct extent_region *rp;
		struct extent *ex = sc->extent_mem;

		for (rp = LIST_FIRST(&ex->ex_regions);
		    rp; rp = LIST_NEXT(rp, er_link))
			;
	}
	{
		struct extent_region *rp;
		struct extent *ex = sc->extent_port;

		for (rp = LIST_FIRST(&ex->ex_regions);
		    rp; rp = LIST_NEXT(rp, er_link))
			;
	}

	if (sc->nbogus == 0)
		return; /* no need to fixup */

	/*
	 * 4. do fixup
	 */
	PCIBIOS_PRINTV((verbose_header, "PCIBIOS fixup stage"));
	sc->nbogus = 0;
	pci_device_foreach(sc, pc, maxbus, pciaddr_resource_allocate);
	PCIBIOS_PRINTV((verbose_footer, sc->nbogus));

}

void
pciaddr_resource_reserve(sc, pc, tag)
	struct pcibr_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	if (pcibr_flags & PCIBR_VERBOSE)
		pciaddr_print_devid(pc, tag);
	pciaddr_resource_manage(sc, pc, tag, pciaddr_do_resource_reserve);
}

void
pciaddr_resource_allocate(sc, pc, tag)
	struct pcibr_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	if (pcibr_flags & PCIBR_VERBOSE)
		pciaddr_print_devid(pc, tag);
	pciaddr_resource_manage(sc, pc, tag, pciaddr_do_resource_allocate);
}

void
pciaddr_resource_manage(sc, pc, tag, func)
	struct pcibr_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	pciaddr_resource_manage_func_t func;
{
	struct extent *ex;
	pcireg_t val, mask;
	bus_addr_t addr;
	bus_size_t size;
	int error, mapreg, type, reg_start, reg_end, width;

	val = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(val)) {
	default:
		printf("WARNING: unknown PCI device header.\n");
		sc->nbogus++;
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
			if (PCI_MAPREG_MEM_TYPE(val) ==
			    PCI_MAPREG_MEM_TYPE_64BIT) {
				/* XXX We could examine the upper 32 bits
				 * XXX of the BAR here, but we are totally
				 * XXX unprepared to handle a non-zero value,
				 * XXX either here or anywhere else in
				 * XXX i386-land.
				 * XXX So just arrange to not look at the
				 * XXX upper 32 bits, lest we misinterpret
				 * XXX it as a 32-bit BAR set to zero.
				 */
				width = 8;
			}
			addr = PCI_MAPREG_MEM_ADDR(val);
			size = PCI_MAPREG_MEM_SIZE(mask);
			ex = sc->extent_mem;
		} else {
			/* XXX some devices give 32bit value */
			addr = (PCI_MAPREG_IO_ADDR(val) & PCIADDR_PORT_END) |
			    sc->sc_iobus_space.bus_base,

			size = PCI_MAPREG_IO_SIZE(mask);
			ex = sc->extent_port;
		}

		if (!size) /* unused register */
			continue;

		/* reservation/allocation phase */
		error += (*func) (sc, pc, tag, mapreg, ex, type, &addr, size);

		PCIBIOS_PRINTV(("\t%02xh %s 0x%08x 0x%08x\n",
		    mapreg, type ? "port" : "mem ",
		    (unsigned int)addr, (unsigned int)size));
	}

	/* enable/disable PCI device */
	val = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	if (error == 0)
		val |= (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
		    PCI_COMMAND_MASTER_ENABLE);
	else
		val &= ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
		     PCI_COMMAND_MASTER_ENABLE);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, val);

	if (error)
		sc->nbogus++;

	PCIBIOS_PRINTV(("\t\t[%s]\n", error ? "NG" : "OK"));
}

int
pciaddr_do_resource_allocate(sc, pc, tag, mapreg, ex, type, addr, size)
	struct pcibr_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	struct extent *ex;
	int mapreg, type;
	bus_addr_t *addr;
	bus_size_t size;
{
	bus_addr_t start;
	int error;

	if (*addr) /* no need to allocate */
		return (0);

	/* XXX Don't allocate if device is AGP device to avoid conflict. */
	if (pciaddr_device_is_agp(pc, tag))
		return (0);

	start = (type == PCI_MAPREG_TYPE_MEM ? sc->sc_membus_space.bus_base
		: sc->sc_iobus_space.bus_base);
	if (start < ex->ex_start || start + size - 1 >= ex->ex_end) {
		PCIBIOS_PRINTV(("No available resources. fixup failed\n"));
		return (1);
	}
	error = extent_alloc_subregion(ex, start, ex->ex_end, size, size, 0, 0,
	    EX_FAST|EX_NOWAIT|EX_MALLOCOK, addr);
	if (error) {
		PCIBIOS_PRINTV(("No available resources. fixup failed\n"));
		return (1);
	}

	/* write new address to PCI device configuration header */
	pci_conf_write(pc, tag, mapreg, *addr);
	/* check */
	if (!pcibr_flags & PCIBR_VERBOSE) {
		printf("pci_addr_fixup: ");
		pciaddr_print_devid(pc, tag);
	}

	if (pciaddr_ioaddr(pci_conf_read(pc, tag, mapreg)) != *addr) {
		pci_conf_write(pc, tag, mapreg, 0); /* clear */
		printf("fixup failed. (new address=%#x)\n", *addr);
		return (1);
	}
	if (!pcibr_flags & PCIBR_VERBOSE)
		printf("new address 0x%08x\n", *addr);

	return (0);
}

int
pciaddr_do_resource_reserve(sc, pc, tag, mapreg, ex, type, addr, size)
	struct pcibr_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	struct extent *ex;
	int type, mapreg;
	bus_addr_t *addr;
	bus_size_t size;
{
	int error;

	if (*addr == 0)
		return (1);

	error = extent_alloc_region(ex, *addr, size, EX_NOWAIT | EX_MALLOCOK);
	if (error) {
		PCIBIOS_PRINTV(("Resource conflict.\n"));
		pci_conf_write(pc, tag, mapreg, 0); /* clear */
		return (1);
	}

	return (0);
}

bus_addr_t
pciaddr_ioaddr(val)
	u_int32_t val;
{
	return ((PCI_MAPREG_TYPE(val) == PCI_MAPREG_TYPE_MEM)
		? PCI_MAPREG_MEM_ADDR(val)
		: (PCI_MAPREG_IO_ADDR(val) & PCIADDR_PORT_END));
}

void
pciaddr_print_devid(pc, tag)
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	int bus, device, function;
	pcireg_t id;

	id = pci_conf_read(pc, tag, PCI_ID_REG);
	pci_decompose_tag(pc, tag, &bus, &device, &function);
	printf("%03d:%02d:%d %04x:%04x\n", bus, device, function,
	    PCI_VENDOR(id), PCI_PRODUCT(id));
}

int
pciaddr_device_is_agp(pc, tag)
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	pcireg_t class, status, rval;
	int off;

	/* Check AGP device. */
	class = pci_conf_read(pc, tag, PCI_CLASS_REG);
	if (PCI_CLASS(class) == PCI_CLASS_DISPLAY) {
		status = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		if (status & PCI_STATUS_CAPLIST_SUPPORT) {
			rval = pci_conf_read(pc, tag, PCI_CAPLISTPTR_REG);
			for (off = PCI_CAPLIST_PTR(rval);
			    off != 0;
			    off = PCI_CAPLIST_NEXT(rval) ) {
				rval = pci_conf_read(pc, tag, off);
				if (PCI_CAPLIST_CAP(rval) == PCI_CAP_AGP)
					return (1);
			}
		}
	}
	return (0);
}


struct extent *
pciaddr_search(mem_port, parent, startp, size)
	int mem_port;
	struct device *parent;
	bus_addr_t *startp;
	bus_size_t size;
{
	struct pcibr_softc *sc;

	/* find the bridge, 'mpcpcibr' */

	sc = NULL;
	while (parent != NULL) {
		if (strncmp("mpcpcibr", parent->dv_xname, 8) == 0) {
			sc = (void *)parent;
			break;
		}
		parent = parent->dv_parent;
	}

	if (sc && !(pcibr_flags & PCIBR_ADDR_FIXUP)) {
		struct extent_region *rp;
		struct extent *ex = mem_port? sc->extent_mem : sc->extent_port;

		/* Search the PCI I/O memory space extent for free
		 * space that will accomodate size.  Remember that the
		 * extent stores allocated space and we're searching
		 * for the gaps.
		 *
		 * If we're at the end or the gap between this region
		 * and the next region big enough, then we're done
		 */
		*startp = ex->ex_start;
		rp = LIST_FIRST(&ex->ex_regions);

		for (rp = LIST_FIRST(&ex->ex_regions);
		    rp && *startp + size > rp->er_start;
		    rp = LIST_NEXT(rp, er_link)) {
			bus_addr_t new_start;

			new_start = (rp->er_end - 1 + size) & ~(size - 1);
			if (new_start > *startp)
				*startp = new_start;
		}

		return (ex);
	}

	return (NULL);
}


void
pci_device_foreach(sc, pc, maxbus, func)
	struct pcibr_softc *sc;
	pci_chipset_tag_t pc;
	int maxbus;
	void (*func)(struct pcibr_softc *, pci_chipset_tag_t, pcitag_t);
{
	const struct pci_quirkdata *qd;
	int bus, device, function, maxdevs, nfuncs;
	pcireg_t id, bhlcr;
	pcitag_t tag;

	for (bus = 0; bus <= maxbus; bus++) {
		maxdevs = pci_bus_maxdevs(pc, bus);
		for (device = 0; device < maxdevs; device++) {
			tag = pci_make_tag(pc, bus, device, 0);
			id = pci_conf_read(pc, tag, PCI_ID_REG);

			/* Invalid vendor ID value? */
			if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
				continue;
			/* XXX Not invalid, but we've done this ~forever. */
			if (PCI_VENDOR(id) == 0)
				continue;

			qd = pci_lookup_quirkdata(PCI_VENDOR(id),
			    PCI_PRODUCT(id));

			bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
			if (PCI_HDRTYPE_MULTIFN(bhlcr) ||
			    (qd != NULL &&
			    (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0))
				nfuncs = 8;
			else
				nfuncs = 1;

			for (function = 0; function < nfuncs; function++) {
				tag = pci_make_tag(pc, bus, device, function);
				id = pci_conf_read(pc, tag, PCI_ID_REG);

				/* Invalid vendor ID value? */
				if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
					continue;
				/*
				 * XXX Not invalid, but we've done this
				 * ~forever.
				 */
				if (PCI_VENDOR(id) == 0)
					continue;
				(*func)(sc, pc, tag);
			}
		}
	}
}
