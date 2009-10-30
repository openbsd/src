/*	$OpenBSD: ip30_machdep.c,v 1.14 2009/10/30 08:13:57 syuu Exp $	*/

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

extern int	mbprint(void *, const char *);

paddr_t	ip30_widget_short(int16_t, u_int);
paddr_t	ip30_widget_long(int16_t, u_int);
paddr_t	ip30_widget_map(int16_t, u_int, bus_addr_t *, bus_size_t *);
int	ip30_widget_id(int16_t, u_int, uint32_t *);

#ifdef MULTIPROCESSOR
static const paddr_t mpconf =
    PHYS_TO_XKPHYS(MPCONF_BASE, CCA_COHERENT_EXCLWRITE);

static int ip30_cpu_exists(int);
#endif

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
 * Autoconf enumeration
 */

void
ip30_autoconf(struct device *parent)
{
	struct mainbus_attach_args maa;

	bzero(&maa, sizeof maa);
	maa.maa_nasid = masternasid;
	maa.maa_name = "cpu";
	config_found(parent, &maa, mbprint);
#ifdef MULTIPROCESSOR
	int cpuid;
	for(cpuid = 1; cpuid < MAX_CPUS; cpuid++)
		if (ip30_cpu_exists(cpuid) == 0)
			config_found(parent, &maa, mbprint);
#endif
	maa.maa_name = "clock";
	config_found(parent, &maa, mbprint);
	maa.maa_name = "xbow";
	config_found(parent, &maa, mbprint);
	maa.maa_name = "power";
	config_found(parent, &maa, mbprint);
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

#ifdef MULTIPROCESSOR
static int
ip30_cpu_exists(int cpuid)
{
       uint32_t magic =
           *(volatile uint32_t *)(mpconf + MPCONF_MAGIC(cpuid));
       if (magic == MPCONF_MAGIC_VAL)
               return 0;
       else
               return 1;
}

void
hw_cpu_boot_secondary(struct cpu_info *ci)
{
       int cpuid =  ci->ci_cpuid;

#ifdef DEBUG
        uint64_t stackaddr =
               *(volatile uint64_t *)(mpconf + MPCONF_STACKADDR(cpuid));
        uint64_t lparam =
               *(volatile uint64_t *)(mpconf + MPCONF_LPARAM(cpuid));
        uint64_t launch =
               *(volatile uint64_t *)(mpconf + MPCONF_LAUNCH(cpuid));
	uint32_t magic =
               *(volatile uint32_t *)(mpconf + MPCONF_MAGIC(cpuid));
        uint32_t prid =
               *(volatile uint32_t *)(mpconf + MPCONF_PRID(cpuid));
        uint32_t physid =
               *(volatile uint32_t *)(mpconf + MPCONF_PHYSID(cpuid));
        uint32_t virtid =
               *(volatile uint32_t *)(mpconf + MPCONF_VIRTID(cpuid));
        uint32_t scachesz =
               *(volatile uint32_t *)(mpconf + MPCONF_SCACHESZ(cpuid));
        uint16_t fanloads =
               *(volatile uint16_t *)(mpconf + MPCONF_FANLOADS(cpuid));
        uint64_t rndvz =
               *(volatile uint64_t *)(mpconf + MPCONF_RNDVZ(cpuid));
        uint64_t rparam =
               *(volatile uint64_t *)(mpconf + MPCONF_RPARAM(cpuid));
        uint32_t idleflag =
               *(volatile uint32_t *)(mpconf + MPCONF_IDLEFLAG(cpuid));

       printf("ci:%p cpuid:%d magic:%x prid:%x physid:%x virtid:%x\n"
           "scachesz:%u fanloads:%x launch:%llx rndvz:%llx\n"
           "stackaddr:%llx lparam:%llx rparam:%llx idleflag:%x\n",
           ci, cpuid, magic, prid, physid, virtid,
           scachesz, fanloads, launch, rndvz,
           stackaddr, lparam, rparam, idleflag);
#endif
       vaddr_t kstack;
       kstack = uvm_km_alloc(kernel_map, USPACE);
       if (kstack == 0) {
               panic("prom_boot_secondary: unable to allocate idle stack");
               return;
       }

       *(volatile uint64_t *)(mpconf + MPCONF_STACKADDR(cpuid)) =
           (uint64_t)(kstack + USPACE);
       *(volatile uint64_t *)(mpconf + MPCONF_LPARAM(cpuid)) =
           (uint64_t)ci;
       *(volatile uint64_t *)(mpconf + MPCONF_LAUNCH(cpuid)) =
           (uint64_t)hw_cpu_spinup_trampoline;

       while(!cpuset_isset(&cpus_running, ci))
	       ;
}

void
hw_cpu_hatch(struct cpu_info *ci)
{
       int cpuid = ci->ci_cpuid;

       /*
        * Make sure we can access the extended address space.
        * Note that r10k and later do not allow XUSEG accesses
        * from kernel mode unless SR_UX is set.
        */
       setsr(getsr() | SR_KX | SR_UX);

       /*
        * Determine system type and set up configuration record data.
        */
       sys_config.cpu[cpuid].clock = sys_config.cpu[0].clock;
       sys_config.cpu[cpuid].type = (cp0_get_prid() >> 8) & 0xff;
       sys_config.cpu[cpuid].vers_maj = (cp0_get_prid() >> 4) & 0x0f;
       sys_config.cpu[cpuid].vers_min = cp0_get_prid() & 0x0f;
       sys_config.cpu[cpuid].fptype = (cp1_get_prid() >> 8) & 0xff;
       sys_config.cpu[cpuid].fpvers_maj = (cp1_get_prid() >> 4) & 0x0f;
       sys_config.cpu[cpuid].fpvers_min = cp1_get_prid() & 0x0f;
       sys_config.cpu[cpuid].tlbsize = 64;

       Mips10k_ConfigCache();

       sys_config.cpu[cpuid].tlbwired = UPAGES / 2;
       tlb_set_wired(0);
       tlb_flush(sys_config.cpu[cpuid].tlbsize);
       tlb_set_wired(sys_config.cpu[cpuid].tlbwired);

       tlb_set_pid(1);

       /*
        * Turn off bootstrap exception vectors.
        */
       setsr(getsr() & ~SR_BOOT_EXC_VEC);

       /*
        * Clear out the I and D caches.
        */
       Mips_SyncCache();

       cpuset_add(&cpus_running, ci);

       for (;;)
               ;
}
#endif
