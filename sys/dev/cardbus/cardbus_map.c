/*	$OpenBSD: cardbus_map.c,v 1.12 2010/03/27 20:04:03 jsg Exp $	*/
/*	$NetBSD: cardbus_map.c,v 1.10 2000/03/07 00:31:46 mycroft Exp $	*/

/*
 * Copyright (c) 1999 and 2000
 *      HAYAKAWA Koichi.  All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/cardbus/cardbusvar.h>

#include <dev/pci/pcireg.h>	/* XXX */

#if defined DEBUG && !defined CARDBUS_MAP_DEBUG
#define CARDBUS_MAP_DEBUG
#endif

#if defined CARDBUS_MAP_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#else
#ifdef DDB
#define STATIC
#else
#define STATIC static
#endif
#define DPRINTF(a)
#endif


STATIC int cardbus_io_find(cardbus_chipset_tag_t, cardbus_function_tag_t,
	       pcitag_t, int, pcireg_t, bus_addr_t *, bus_size_t *,
	       int *);
STATIC int cardbus_mem_find(cardbus_chipset_tag_t, cardbus_function_tag_t,
	       pcitag_t, int, pcireg_t, bus_addr_t *, bus_size_t *,
	       int *);

int
cardbus_mapreg_probe(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    pcitag_t tag, int reg, pcireg_t *typep)
{
	pcireg_t address, mask;
	int s;

	s = splhigh();
	address = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, 0xffffffff);
	mask = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, address);
	splx(s);

	if (mask == 0) /* unimplemented mapping register */
		return (0);

	if (typep)
		*typep = _PCI_MAPREG_TYPEBITS(address);
	return (1);
}

/*
 * STATIC int cardbus_io_find(cardbus_chipset_tag_t cc,
 *			      cardbus_function_tag_t cf, pcitag_t tag,
 *			      int reg, pcireg_t type, bus_addr_t *basep,
 *			      bus_size_t *sizep, int *flagsp)
 * This code is stolen from sys/dev/pci_map.c.
 */
STATIC int
cardbus_io_find(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    pcitag_t tag, int reg, pcireg_t type, bus_addr_t *basep,
    bus_size_t *sizep, int *flagsp)
{
	pcireg_t address, mask;
	int s;

	/* EXT ROM is able to map on memory space ONLY. */
	if (reg == CARDBUS_ROM_REG)
		return (1);

	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3)) {
		panic("cardbus_io_find: bad request");
	}

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
	address = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, 0xffffffff);
	mask = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, address);
	splx(s);

	if (PCI_MAPREG_TYPE(address) != PCI_MAPREG_TYPE_IO) {
		printf("cardbus_io_find: expected type i/o, found mem\n");
		return (1);
	}

	if (PCI_MAPREG_IO_SIZE(mask) == 0) {
		printf("cardbus_io_find: void region\n");
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

/*
 * STATIC int cardbus_mem_find(cardbus_chipset_tag_t cc,
 *			       cardbus_function_tag_t cf, pcitag_t tag,
 *			       int reg, pcireg_t type, bus_addr_t *basep,
 *			       bus_size_t *sizep, int *flagsp)
 * This code is stolen from sys/dev/pci_map.c.
 */
STATIC int
cardbus_mem_find(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    pcitag_t tag, int reg, pcireg_t type, bus_addr_t *basep,
    bus_size_t *sizep, int *flagsp)
{
	pcireg_t address, mask;
	int s;

	if (reg != CARDBUS_ROM_REG &&
	    (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3))) {
		panic("cardbus_mem_find: bad request");
	}

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
	address = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, 0xffffffff);
	mask = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, address);
	splx(s);

	if (reg != CARDBUS_ROM_REG) {
		/* memory space BAR */

		if (PCI_MAPREG_TYPE(address) != PCI_MAPREG_TYPE_MEM) {
			printf("cardbus_mem_find: expected type mem, "
			    "found i/o\n");
			return (1);
		}
		if (PCI_MAPREG_MEM_TYPE(address) != PCI_MAPREG_MEM_TYPE(type)) {
			printf("cardbus_mem_find: expected mem type %08x, "
			    "found %08x\n", PCI_MAPREG_MEM_TYPE(type),
			    PCI_MAPREG_MEM_TYPE(address));
			return (1);
		}
	}

	if (PCI_MAPREG_MEM_SIZE(mask) == 0) {
		printf("cardbus_mem_find: void region\n");
		return (1);
	}

	switch (PCI_MAPREG_MEM_TYPE(address)) {
	case PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_MEM_TYPE_32BIT_1M:
		break;
	case PCI_MAPREG_MEM_TYPE_64BIT:
		printf("cardbus_mem_find: 64-bit memory mapping register\n");
		return (1);
	default:
		printf("cardbus_mem_find: reserved mapping register type\n");
		return (1);
	}

	if (basep != 0)
		*basep = PCI_MAPREG_MEM_ADDR(address);
	if (sizep != 0)
		*sizep = PCI_MAPREG_MEM_SIZE(mask);
	if (flagsp != 0) {
		*flagsp =
#ifdef BUS_SPACE_MAP_PREFETCHABLE
		    PCI_MAPREG_MEM_PREFETCHABLE(address) ?
		      BUS_SPACE_MAP_PREFETCHABLE :
#endif
		0;
	}

	return (0);
}

/*
 * int cardbus_mapreg_map(struct cardbus_softc *, int, int, pcireg_t,
 *			  int bus_space_tag_t *, bus_space_handle_t *,
 *			  bus_addr_t *, bus_size_t *)
 *    This function maps bus-space on the value of Base Address
 *   Register (BAR) indexed by the argument `reg' (the second argument).
 *   When the value of the BAR is not valid, such as 0x00000000, a new
 *   address should be allocated for the BAR and new address values is
 *   written on the BAR.
 */
int
cardbus_mapreg_map(struct cardbus_softc *sc, int func, int reg,
    pcireg_t type, int busflags, bus_space_tag_t *tagp,
    bus_space_handle_t *handlep, bus_addr_t *basep, bus_size_t *sizep)
{
	cardbus_chipset_tag_t cc = sc->sc_cc;
	cardbus_function_tag_t cf = sc->sc_cf;
	bus_space_tag_t bustag;
	rbus_tag_t rbustag;
	bus_space_handle_t handle;
	bus_addr_t base;
	bus_size_t size;
	int flags;
	int status = 0;

	pcitag_t tag = cardbus_make_tag(cc, cf, sc->sc_bus,
	    sc->sc_device, func);

	DPRINTF(("cardbus_mapreg_map called: %s %x\n", sc->sc_dev.dv_xname,
	   type));

	if (PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_IO) {
		if (cardbus_io_find(cc, cf, tag, reg, type, &base, &size,
		    &flags))
			status = 1;
		bustag = sc->sc_iot;
		rbustag = sc->sc_rbus_iot;
	} else {
		if (cardbus_mem_find(cc, cf, tag, reg, type, &base, &size,
		    &flags))
			status = 1;
		bustag = sc->sc_memt;
		rbustag = sc->sc_rbus_memt;
	}
	if (status == 0) {
		bus_addr_t mask = size - 1;
		if (base != 0)
			mask = 0xffffffff;
		if ((*cf->cardbus_space_alloc)(cc, rbustag, base, size, mask,
		    size, busflags | flags, &base, &handle)) {
			panic("io alloc");
		}
	}
	cardbus_conf_write(cc, cf, tag, reg, base);

	DPRINTF(("cardbus_mapreg_map: physaddr %lx\n", (unsigned long)base));

	if (tagp != 0)
		*tagp = bustag;
	if (handlep != 0)
		*handlep = handle;
	if (basep != 0)
		*basep = base;
	if (sizep != 0)
		*sizep = size;
	cardbus_free_tag(cc, cf, tag);

	return (0);
}

/*
 * int cardbus_mapreg_unmap(struct cardbus_softc *sc, int func, int reg,
 *			    bus_space_tag_t tag, bus_space_handle_t handle,
 *			    bus_size_t size)
 *
 *   This function releases bus-space region and close memory or io
 *   window on the bridge.
 *
 *  Arguments:
 *   struct cardbus_softc *sc; the pointer to the device structure of cardbus.
 *   int func; the number of function on the device.
 *   int reg; the offset of BAR register.
 */
int
cardbus_mapreg_unmap(struct cardbus_softc *sc, int func, int reg,
    bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t size)
{
	cardbus_chipset_tag_t cc = sc->sc_cc;
	cardbus_function_tag_t cf = sc->sc_cf;
	int st = 1;
	pcitag_t cardbustag;
	rbus_tag_t rbustag;

	if (sc->sc_iot == tag) {
		/* bus space is io space */
		DPRINTF(("%s: unmap i/o space\n", sc->sc_dev.dv_xname));
		rbustag = sc->sc_rbus_iot;
	} else if (sc->sc_memt == tag) {
		/* bus space is memory space */
		DPRINTF(("%s: unmap mem space\n", sc->sc_dev.dv_xname));
		rbustag = sc->sc_rbus_memt;
	} else
		return (1);

	cardbustag = cardbus_make_tag(cc, cf, sc->sc_bus, sc->sc_device, func);

	cardbus_conf_write(cc, cf, cardbustag, reg, 0);

	(*cf->cardbus_space_free)(cc, rbustag, handle, size);

	cardbus_free_tag(cc, cf, cardbustag);

	return (st);
}
