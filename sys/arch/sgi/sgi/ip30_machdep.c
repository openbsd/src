/*	$OpenBSD: ip30_machdep.c,v 1.13 2009/10/22 22:08:54 miod Exp $	*/

/*
 * Copyright (c) 2008, 2009 Miodrag Vallat.
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
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbridgereg.h>

#include <sgi/xbow/xheartreg.h>
#include <sgi/pci/iocreg.h>

#include <dev/ic/comvar.h>

extern char *hw_prod;

paddr_t	ip30_widget_short(int16_t, u_int);
paddr_t	ip30_widget_long(int16_t, u_int);
paddr_t	ip30_widget_map(int16_t, u_int, bus_addr_t *, bus_size_t *);
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
	paddr_t iocbase;
	u_long cpuspeed;

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

	xbow_widget_base = ip30_widget_short;
	xbow_widget_map = ip30_widget_map;
	xbow_widget_id = ip30_widget_id;

	cpuspeed = bios_getenvint("cpufreq");
	if (cpuspeed < 100)
		cpuspeed = 175;		/* reasonable default */
	sys_config.cpu[0].clock = cpuspeed * 1000000;

	/*
	 * Initialize the early console parameters.
	 * On Octane, the BRIDGE is always widet 15, and IOC3 is always
	 * mapped in memory space at address 0x500000.
	 *
	 * Also, note that by using a direct widget bus_space, there is
	 * no endianness conversion done on the bus addresses. Which is
	 * exactly what we need, since the IOC3 doesn't need any. Some
	 * may consider this an evil abuse of bus_space knowledge, though.
	 */

	xbow_build_bus_space(&sys_config.console_io, 0, 15);
	sys_config.console_io.bus_base = ip30_widget_long(0, 15) +
	    BRIDGE_PCI0_MEM_SPACE_BASE;

	comconsaddr = 0x500000 + IOC3_UARTA_BASE;
	comconsfreq = 22000000 / 3;
	comconsiot = &sys_config.console_io;
	comconsrate = bios_getenvint("dbaud");
	if (comconsrate < 50 || comconsrate > 115200)
		comconsrate = 9600;

	/*
	 * Octane and Octane2 can be told apart with a GPIO source bit
	 * in the onboard IOC3.
	 */
	iocbase = ip30_widget_short(0, 15) + 0x500000;
	if (*(volatile uint32_t *)(iocbase + IOC3_GPPR(IP30_GPIO_CLASSIC)) != 0)
		hw_prod = "Octane";
	else
		hw_prod = "Octane2";
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

paddr_t
ip30_widget_map(int16_t nasid, u_int widget, bus_addr_t *offs, bus_size_t *len)
{
	paddr_t base;

	/*
	 * On Octane, the whole widget space is always accessible.
	 */

	base = ip30_widget_long(nasid, widget);
	*len = (1ULL << 36) - *offs;

	return base + *offs;
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
		if (!ISSET(*(uint32_t *)(linkpa + (WIDGET_LINK_STATUS | 4)),
		    WIDGET_STATUS_ALIVE))
			return ENXIO;	/* not connected */
	}

	wpa = ip30_widget_short(nasid, widget);
	if (wid != NULL)
		*wid = *(uint32_t *)(wpa + (WIDGET_ID | 4));

	return 0;
}
