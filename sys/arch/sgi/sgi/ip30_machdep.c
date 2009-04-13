/*	$OpenBSD: ip30_machdep.c,v 1.4 2009/04/13 21:17:54 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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
 * Octane (IP30) specific code.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <mips64/arcbios.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/memconf.h>

#include <uvm/uvm_extern.h>

#include <sgi/sgi/ip30.h>
#include <sgi/xbow/widget.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbridgereg.h>

#include <sgi/xbow/xheartreg.h>
#include <sgi/pci/iocreg.h>

#include <dev/ic/comvar.h>

paddr_t	ip30_widget_short(int16_t, u_int);
paddr_t	ip30_widget_long(int16_t, u_int);
int	ip30_widget_id(int16_t, u_int, uint32_t *);

void
ip30_setup()
{
#if 0
	paddr_t heart;
	int bank;
	uint32_t memcfg;
	uint64_t start, count;
#endif

	/*
	 * Although being r10k/r12k based, the uncached spaces are
	 * apparently not used in this design.
	 */
	uncached_base = PHYS_TO_XKPHYS(0, CCA_NC);

#if 0
	/*
	 * Scan for memory. ARCBios reports at least up to 2GB; if
	 * memory above 2GB isn't reported, we'll need to re-enable this
	 * code and add the unseen areas.
	 */
	heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC);
	for (bank = 0; bank < 8; bank++) {
		memcfg = *(uint32_t *)
		    (heart + HEART_MEMORY_STATUS + bank * sizeof(uint32_t));
		bios_printf("memory bank %d: %08x\n", bank, memcfg);

		if (!ISSET(memcfg, HEART_MEMORY_VALID))
			continue;

		count = ((memcfg & HEART_MEMORY_SIZE_MASK) >>
		    HEART_MEMORY_SIZE_SHIFT) + 1;
		start = (memcfg & HEART_MEMORY_ADDR_MASK) >>
		    HEART_MEMORY_ADDR_SHIFT;

		count <<= HEART_MEMORY_UNIT_SHIFT;
		start <<= HEART_MEMORY_UNIT_SHIFT;

		/* Physical memory starts at 512MB */
		start += IP30_MEMORY_BASE;
		bios_printf("memory from %p to %p\n",
		    ptoa(start), ptoa(start + count));
	}
#endif

	xbow_widget_short = ip30_widget_short;
	xbow_widget_long = ip30_widget_long;
	xbow_widget_id = ip30_widget_id;

	/*
	 * Initialize the early console parameters.
	 * This assumes BRIDGE is on widget 15 and IOC3 is mapped in
	 * memory space at address 0x500000.
	 *
	 * XXX And that 0x500000 should be computed from the first BAR
	 * XXX of the IOC3 in pci configuration space. Joy. I'll get there
	 * XXX eventually.
	 *
	 * Also, note that by using a direct widget bus_space, there is
	 * no endianness conversion done on the bus addresses. Which is
	 * exactly what we need, since the IOC3 doesn't need any. Some
	 * may consider this an evil abuse of bus_space knowledge, though.
	 */
	xbow_build_bus_space(&sys_config.console_io, 0, 15, 1);
	sys_config.console_io.bus_base += BRIDGE_PCI_MEM_SPACE_BASE;

	comconsaddr = 0x500000 + IOC3_UARTA_BASE;
	comconsfreq = 22000000 / 3;
	comconsiot = &sys_config.console_io;
}

/*
 * Widget mapping. IP30 only has one processor board node, so the nasid
 * parameter is ignored.
 */

paddr_t
ip30_widget_short(int16_t nasid, u_int widget)
{
	return ((uint64_t)(widget) << 24) | (1ULL << 28) | uncached_base;
}

paddr_t
ip30_widget_long(int16_t nasid, u_int widget)
{
	return ((uint64_t)(widget) << 36) | uncached_base;
}

/*
 * Widget enumeration
 */

int
ip30_widget_id(int16_t nasid, u_int widget, uint32_t *wid)
{
	paddr_t linkpa, wpa;

	if (widget != 0)
	{
		if (widget < WIDGET_MIN || widget > WIDGET_MAX)
			return EINVAL;

		linkpa = ip30_widget_short(nasid, 0) + XBOW_WIDGET_LINK(widget);
		if (!ISSET(*(uint32_t *)(linkpa + WIDGET_LINK_STATUS),
		    WIDGET_STATUS_ALIVE))
			return ENXIO;	/* not connected */
	}

	wpa = ip30_widget_short(nasid, widget);
	if (wid != NULL)
		*wid = *(uint32_t *)(wpa + WIDGET_ID);

	return 0;
}

void
hw_setintrmask(intrmask_t m)
{
	extern intrmask_t heart_intem;

	paddr_t heart;
	heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC);
	*(volatile uint64_t *)(heart + HEART_IMR(0)) = heart_intem & ~m;
}
